#include "libFirmwareUpdate++/dfu.hpp"
#include "DfuFinderImpl.hpp"
#include "libFirmwareUpdate++/dfu/DfuInterface.hpp"
#include "ContextImpl.hpp"
#include "usb_dfu.hpp"
#include "quirks.hpp"

#include <sstream>
#include <cstring>

/* USB string descriptor should contain max 126 UTF-16 characters
 * but 253 would even accomodate any UTF-8 encoding */
#define MAX_DESC_STR_LEN 253

namespace FwUpd
{

void DfuFinderImpl::find()
{
	results->clear();
	libusb_device **list = nullptr;
	ssize_t num_devs;
	ssize_t i;

	num_devs = libusb_get_device_list(usbctx, &list);
	for (i = 0; i < num_devs; ++i) {
		struct libusb_device_descriptor desc;
		struct libusb_device *dev = list[i];

		if (f->match_path!="" && get_path(dev) != f->match_path)
			continue;
		if (libusb_get_device_descriptor(dev, &desc))
			continue;
		probe_configuration(dev, &desc);
	}
	libusb_free_device_list(list, 0);
}

/*
 * Look for a descriptor in a concatenated descriptor list. Will
 * return upon the first match of the given descriptor type. Returns length of
 * found descriptor, limited to res_size
 */
int DfuFinderImpl::find_descriptor(const uint8_t *desc_list, int list_len, uint8_t desc_type, void *res_buf, int res_size)
{
	int p = 0;

	if (list_len < 2)
		return (-1);

	while (p + 1 < list_len) {
		int desclen;

		desclen = (int) desc_list[p];
		if (desclen == 0) {
			ctxi->log(LogLevel::Warn, "Invalid descriptor list");
			return -1;
		}
		if (desc_list[p + 1] == desc_type) {
			if (desclen > res_size)
				desclen = res_size;
			if (p + desclen > list_len)
				desclen = list_len - p;
			memcpy(res_buf, &desc_list[p], desclen);
			return desclen;
		}
		p += (int) desc_list[p];
	}
	return -1;

}

void DfuFinderImpl::probe_configuration(libusb_device *dev, libusb_device_descriptor *desc)
{
	UsbDfuFuncDescriptor func_dfu;
	libusb_device_handle *devh;
	struct libusb_config_descriptor *cfg;
	const struct libusb_interface_descriptor *intf;
	const struct libusb_interface *uif;
	char alt_name[MAX_DESC_STR_LEN + 1];
	char serial_name[MAX_DESC_STR_LEN + 1];
	int cfg_idx;
	int intf_idx;
	int alt_idx;
	int ret;
	int has_dfu;

	for (cfg_idx = 0; cfg_idx != desc->bNumConfigurations; cfg_idx++) {
		func_dfu.clear();
		has_dfu = 0;

		ret = libusb_get_config_descriptor(dev, cfg_idx, &cfg);
		if (ret != 0)
			return;
		if (f->match_config_index > -1 && f->match_config_index != cfg->bConfigurationValue) {
			libusb_free_config_descriptor(cfg);
			continue;
		}

		/*
			 * In some cases, noticably FreeBSD if uid != 0,
			 * the configuration descriptors are empty
			 */
		if (!cfg)
			return;

		uint8_t funcDfu_data[UsbDfuFuncDescriptor::sizeInBytes()];

		ret = find_descriptor(cfg->extra, cfg->extra_length,
							  USB_DT_DFU, funcDfu_data, sizeof(funcDfu_data));
		if (ret > -1)
		{
			func_dfu.parse(funcDfu_data, ret);
			goto found_dfu;
		}

		for (intf_idx = 0; intf_idx < cfg->bNumInterfaces;
			 intf_idx++) {
			uif = &cfg->interface[intf_idx];
			if (!uif)
				break;

			for (alt_idx = 0; alt_idx < cfg->interface[intf_idx].num_altsetting;
				 alt_idx++) {
				intf = &uif->altsetting[alt_idx];

				if (intf->bInterfaceClass != 0xfe ||
						intf->bInterfaceSubClass != 1)
					continue;

				ret = find_descriptor(intf->extra, intf->extra_length, USB_DT_DFU,
									  funcDfu_data, sizeof(funcDfu_data));
				if (ret > -1)
				{
					func_dfu.parse(funcDfu_data, ret);
					goto found_dfu;
				}

				has_dfu = 1;
			}
		}
		if (has_dfu) {
			/*
				 * Finally try to retrieve it requesting the
				 * device directly This is not supported on
				 * all devices for non-standard types
				 */
			if (libusb_open(dev, &devh) == 0) {
				ret = libusb_get_descriptor(devh, USB_DT_DFU, 0,
											funcDfu_data, sizeof(funcDfu_data));
				libusb_close(devh);
				if (ret > -1)
				{
					func_dfu.parse(funcDfu_data, ret);
					goto found_dfu;
				}
			}
			ctxi->log(LogLevel::Warn, "Device has DFU interface, "
									  "but has no DFU functional descriptor");

			/* fake version 1.0 */
			func_dfu.bLength = 7;
			func_dfu.bcdDFUVersion = 0x0100;
			goto found_dfu;
		}
		libusb_free_config_descriptor(cfg);
		continue;

found_dfu:
		if (func_dfu.bLength == 7) {
			ctxi->logf(LogLevel::Info, "Deducing device DFU version from functional descriptor "
				   "length");
			func_dfu.bcdDFUVersion = 0x0100;
		} else if (func_dfu.bLength < 9) {
			ctxi->logf(LogLevel::Warn, "Error obtaining DFU functional descriptor. Please report this as a bug!");
			ctxi->logf(LogLevel::Warn, "Assuming DFU version 1.0");
			func_dfu.bcdDFUVersion = 0x0100;
			ctxi->logf(LogLevel::Warn, "Transfer size can not be detected");
			func_dfu.wTransferSize = 0;
		}

		for (intf_idx = 0; intf_idx < cfg->bNumInterfaces;
			 intf_idx++) {
			if (f->match_iface_index > -1 && f->match_iface_index != intf_idx)
				continue;

			uif = &cfg->interface[intf_idx];
			if (!uif)
				break;

			for (alt_idx = 0;
				 alt_idx < uif->num_altsetting; alt_idx++) {
				int dfu_mode;

				intf = &uif->altsetting[alt_idx];

				if (intf->bInterfaceClass != 0xfe ||
						intf->bInterfaceSubClass != 1)
					continue;

				dfu_mode = (intf->bInterfaceProtocol == 2);
				/* e.g. DSO Nano has bInterfaceProtocol 0 instead of 2 */
				if (func_dfu.bcdDFUVersion == 0x011a && intf->bInterfaceProtocol == 0)
					dfu_mode = 1;

				/* LPC DFU bootloader has bInterfaceProtocol 1 (Runtime) instead of 2 */
				if (desc->idVendor == 0x1fc9 && desc->idProduct == 0x000c && intf->bInterfaceProtocol == 1)
					dfu_mode = 1;

				if (dfu_mode &&
						f->match_iface_alt_index > -1 && f->match_iface_alt_index != alt_idx)
					continue;

				UsbId usbId(desc->idVendor, desc->idProduct);
				if (dfu_mode) {
					if (!usbId.matchesSearch(f->match_usbId_dfu))
						continue;
				} else {
					if (f->matchDfuOnly || !usbId.matchesSearch(f->match_usbId))
						continue;
				}

				if (libusb_open(dev, &devh)) {
					std::string logFmtStr = "Cannot open DFU device %04x:%04x";
#if (defined(__MINGW32__) || defined(_WIN32) || defined(_WIN64))
					// Failure to open the device on Windows may mean that the correct driver (i.e. something libusb can use) has not been selected/installed
					logFmtStr += " \nPlease check that you have installed the correct driver.";
#endif
					ctxi->logf(LogLevel::Warn, logFmtStr.data(), desc->idVendor, desc->idProduct);
					break;
				}
				if (intf->iInterface != 0)
					ret = libusb_get_string_descriptor_ascii(devh,
															 intf->iInterface, (unsigned char*)alt_name, MAX_DESC_STR_LEN);
				else
					ret = -1;
				if (ret < 1)
					strcpy(alt_name, "UNKNOWN");
				if (desc->iSerialNumber != 0)
					ret = libusb_get_string_descriptor_ascii(devh,
															 desc->iSerialNumber, (unsigned char*)serial_name, MAX_DESC_STR_LEN);
				else
					ret = -1;
				if (ret < 1)
					strcpy(serial_name, "UNKNOWN");
				libusb_close(devh);

				if (dfu_mode &&
						f->match_iface_alt_name != "" && f->match_iface_alt_name != alt_name)
					continue;

				if (dfu_mode) {
					if (f->match_serial_dfu != "" && f->match_serial_dfu != serial_name)
						continue;
				} else {
					if (f->match_serial != "" && f->match_serial != serial_name)
						continue;
				}

				auto pdfu = std::make_shared<DfuInterface>();

				pdfu->ctx = f->ctx;
				pdfu->func_dfu = func_dfu;
				pdfu->dev = libusb_ref_device(dev);
				pdfu->quirks = get_quirks(desc->idVendor,
										  desc->idProduct, desc->bcdDevice);
				pdfu->usbId.vendor = desc->idVendor;
				pdfu->usbId.product = desc->idProduct;
				pdfu->bcdDevice = desc->bcdDevice;
				pdfu->configuration = cfg->bConfigurationValue;
				pdfu->interface = intf->bInterfaceNumber;
				pdfu->altsetting = intf->bAlternateSetting;
				pdfu->devnum = libusb_get_device_address(dev);
				pdfu->busnum = libusb_get_bus_number(dev);
				pdfu->alt_name = alt_name;
				pdfu->serial_name = serial_name;
				if (dfu_mode)
					pdfu->flags |= DFU_IFF_DFU;
				if (pdfu->quirks & QUIRK_FORCE_DFU11) {
					pdfu->func_dfu.bcdDFUVersion =
							0x0110;
				}
				pdfu->bMaxPacketSize0 = desc->bMaxPacketSize0;

				results->push_back(std::move(pdfu));
			}
		}
		libusb_free_config_descriptor(cfg);
	}
}

std::string DfuFinderImpl::get_path(libusb_device *dev)
{
	std::ostringstream ss;
	uint8_t path[8];
	int portCount = libusb_get_port_numbers(dev, path, sizeof(path));
	if (portCount > 0)
	{
		ss << static_cast<int>(libusb_get_bus_number(dev));
		ss << "-";
		ss << static_cast<int>(path[0]);
		for (int j=0; j<portCount; j++)
		{
			if (j==0)
				ss << "-";
			else
				ss << ".";
			ss << static_cast<int>(path[j]);
		}
	}
	return ss.str();
}



}
