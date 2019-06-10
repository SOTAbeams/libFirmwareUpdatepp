#ifndef fwupd_dfu_DfuController_h
#define fwupd_dfu_DfuController_h

#include "libFirmwareUpdate++/dfu.hpp"
#include <cstdint>

namespace FwUpd
{


class DfuController
{
protected:
	ContextImpl *ctxi() const;
	uint32_t getDefaultTransferSize();
	void calcTransferSize();
	int transferSize;
public:
	std::shared_ptr<DfuInterface> dif = nullptr;
	uint32_t transferSizeOverride = 0;
	void abortToIdle();

	DfuController(std::shared_ptr<DfuInterface> dif);
};

class DfuController_download: public DfuController
{
public:
	std::shared_ptr<DfuFile> file;
	int run();
	using DfuController::DfuController;
};

}

#endif
