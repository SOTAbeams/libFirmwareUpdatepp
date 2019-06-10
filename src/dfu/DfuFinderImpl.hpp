#ifndef fwupd_dfu_FinderImpl_h
#define fwupd_dfu_FinderImpl_h

#include "libFirmwareUpdate++/dfu.hpp"
#include <libusb.h>

namespace FwUpd
{

class DfuFinderImpl
{
public:
	ContextImpl *ctxi;
	libusb_context *usbctx;
	DfuFinder *f;
	DfuFinder::Results *results;

	void find();
protected:
	int find_descriptor(const uint8_t *desc_list, int list_len,
		uint8_t desc_type, void *res_buf, int res_size);
	void probe_configuration(libusb_device *dev, libusb_device_descriptor *desc);
public:
	// TODO: move elsewhere?
	static std::string get_path(libusb_device *dev);

};

}

#endif
