#ifndef libFirmwareUpdate_dfu_DfuFinder_h
#define libFirmwareUpdate_dfu_DfuFinder_h

#include "libFirmwareUpdate++/Context.hpp"
#include "libFirmwareUpdate++/UsbId.hpp"
#include "libFirmwareUpdate++/dfu/DfuInterface.hpp"

#include <string>
#include <vector>

namespace FwUpd
{

class DfuFinder
{
public:
	std::shared_ptr<Context> ctx;

	std::string match_path;
	UsbId match_usbId;
	UsbId match_usbId_dfu;
	int match_config_index;
	int match_iface_index;
	int match_iface_alt_index;
	std::string match_iface_alt_name;
	std::string match_serial;
	std::string match_serial_dfu;
	bool matchDfuOnly;

	using Results = std::vector<std::shared_ptr<DfuInterface>>;
	Results find();

	DfuFinder(std::shared_ptr<Context> ctx);
	virtual ~DfuFinder();
	void reset();
};

}

#endif
