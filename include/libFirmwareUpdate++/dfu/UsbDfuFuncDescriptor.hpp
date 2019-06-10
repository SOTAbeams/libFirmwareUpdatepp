#ifndef libFirmwareUpdate_dfu_UsbDfuFuncDescriptor_h
#define libFirmwareUpdate_dfu_UsbDfuFuncDescriptor_h

#include <cstdint>
#include <cstdlib>

namespace FwUpd
{

class UsbDfuFuncDescriptor
{
public:
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint8_t		bmAttributes;
	uint16_t		wDetachTimeOut;
	uint16_t		wTransferSize;
	uint16_t		bcdDFUVersion;

	bool attr_canDownload() const;
	bool attr_canUpload() const;
	bool attr_manifestTol() const;
	bool attr_willDetach() const;

	void clear();
	bool parse(const uint8_t *data, size_t length);
	static int sizeInBytes();
};

}

#endif
