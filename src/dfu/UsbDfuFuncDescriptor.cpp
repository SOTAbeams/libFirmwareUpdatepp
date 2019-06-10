#include "libFirmwareUpdate++/dfu/UsbDfuFuncDescriptor.hpp"
#include "PackedData.hpp"

#define USB_DFU_CAN_DOWNLOAD	(1 << 0)
#define USB_DFU_CAN_UPLOAD	(1 << 1)
#define USB_DFU_MANIFEST_TOL	(1 << 2)
#define USB_DFU_WILL_DETACH	(1 << 3)

namespace FwUpd
{

bool UsbDfuFuncDescriptor::attr_canDownload() const
{
	return (bmAttributes & USB_DFU_CAN_DOWNLOAD);
}

bool UsbDfuFuncDescriptor::attr_canUpload() const
{
	return (bmAttributes & USB_DFU_CAN_UPLOAD);
}

bool UsbDfuFuncDescriptor::attr_manifestTol() const
{
	return (bmAttributes & USB_DFU_MANIFEST_TOL);
}

bool UsbDfuFuncDescriptor::attr_willDetach() const
{
	return (bmAttributes & USB_DFU_WILL_DETACH);
}

void UsbDfuFuncDescriptor::clear()
{
	bLength = 0;
	bDescriptorType = 0;
	bmAttributes = 0;
	wDetachTimeOut = 0;
	wTransferSize = 0;
	bcdDFUVersion = 0;
}

bool UsbDfuFuncDescriptor::parse(const uint8_t *data, size_t length)
{
	clear();
	try
	{
		PackedData::Reader d(data, length);
		bLength = d.read_u8();
		bDescriptorType = d.read_u8();
		bmAttributes = d.read_u8();
		wDetachTimeOut = d.read_u16l();
		wTransferSize = d.read_u16l();
		bcdDFUVersion = d.read_u16l();
	}
	catch (PackedData::error_OutOfRange &e)
	{
		// Soft fail if data is too short.
		// All unfilled fields are set to 0 by the clear() at the beginning of this function
		return false;
	}
	return true;
}

int UsbDfuFuncDescriptor::sizeInBytes()
{
	return 9;
}

}
