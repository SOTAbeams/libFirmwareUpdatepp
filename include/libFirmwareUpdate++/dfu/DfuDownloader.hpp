#ifndef libFirmwareUpdate_dfu_DfuDownloader_h
#define libFirmwareUpdate_dfu_DfuDownloader_h

#include "libFirmwareUpdate++/dfu/DfuFile.hpp"
#include "libFirmwareUpdate++/dfu/DfuFinder.hpp"
#include "libFirmwareUpdate++/dfu/DfuseOptions.hpp"
#include <memory>

namespace FwUpd
{

class DfuDownloader
{
public:
	std::shared_ptr<Context> ctx;
	std::shared_ptr<DfuFile> file;
	DfuFinder probe;
	bool forceDfuse = false;
	bool finalReset = false;
	std::shared_ptr<DfuseOptions> dfuseOpts = std::make_shared<DfuseOptions>();
	bool run();

	DfuDownloader(std::shared_ptr<Context> ctx, std::shared_ptr<DfuFile> file);
};

}

#endif
