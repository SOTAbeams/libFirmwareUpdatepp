#ifndef libFirmwareUpdate_dfu_DfuInterface_h
#define libFirmwareUpdate_dfu_DfuInterface_h

#include "libFirmwareUpdate++/Context.hpp"
#include "libFirmwareUpdate++/UsbId.hpp"
#include "libFirmwareUpdate++/dfu/UsbDfuFuncDescriptor.hpp"

#include <string>

class libusb_device;
class libusb_device_handle;

struct dfu_status {
    unsigned char bStatus;
    unsigned int  bwPollTimeout;
    unsigned char bState;
    unsigned char iString;
};

namespace FwUpd
{

// TODO: hide some of this in an impl class?
class DfuInterface
{
public:
	std::shared_ptr<Context> ctx;

	UsbDfuFuncDescriptor func_dfu;
    uint16_t quirks;
    uint16_t busnum;
    uint16_t devnum;
	UsbId usbId;
    uint16_t bcdDevice;
    uint8_t configuration;
    uint8_t interface;
    uint8_t altsetting;
    uint8_t flags = 0;
    uint8_t bMaxPacketSize0;
    std::string alt_name;
    std::string serial_name;

    libusb_device *dev = nullptr;
    libusb_device_handle *dev_handle = nullptr;
	bool isOpen;
	bool isClaimed;

public:
	int dfuXferIn(uint8_t bRequest, uint16_t wValue, unsigned char *data, uint16_t wLength);
	int dfuXferOut(uint8_t bRequest, uint16_t wValue, unsigned char *data, uint16_t wLength);

	int download(uint16_t blockNum, unsigned char *data, uint16_t length);
	int upload(uint16_t blockNum, unsigned char *data, uint16_t length);
	int detach(uint16_t timeout);
	int getStatus(struct dfu_status *status);
	int clearStatus();
	int getState();
	int abort();

	void openDevice();
	void closeDevice();
	void claimInterface();
	void releaseInterface();


	// TODO: delete copy/assign
	virtual ~DfuInterface();
};

}

#endif
