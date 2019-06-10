#ifndef fwupd_dfuse_DfuseController_h
#define fwupd_dfuse_DfuseController_h

#include "libFirmwareUpdate++/dfu.hpp"
#include "libFirmwareUpdate++/dfu/DfuseOptions.hpp"

#include "dfu/DfuController.hpp"
#include "MemLayout.hpp"

#include <cstdint>
#include <memory>

namespace FwUpd
{

enum class DfuseCommand
{
	SetAddress,
	ErasePage,
	MassErase,
	ReadUnprotect,
};
const char * DfuseCommand_toString(DfuseCommand cmd);


class DfuseController : public DfuController
{
protected:
public:
	unsigned int last_erased_page = 1; /* non-aligned value, won't match */
	Dfuse::MemLayout memLayout;
	unsigned int dfuse_address = 0;
	std::shared_ptr<DfuseOptions> opts = std::make_shared<DfuseOptions>();

	int specialCommand(unsigned int address, DfuseCommand command);
	int req_upload(const unsigned short length, unsigned char *data, unsigned short transaction);
	int req_dnload(const unsigned short length, unsigned char *data, unsigned short transaction);
	using DfuController::DfuController;
};

class DfuseController_download : public DfuseController
{
protected:
	void progress(const uint8_t *dataPos);
	int dnload_chunk(const uint8_t *data, int size, int transaction);
	int dnload_element(unsigned int dwElementAddress,
					   unsigned int dwElementSize, const uint8_t *data);
	int dnload_dfuseFile();

public:
	std::shared_ptr<DfuFile> file;
	int run();
	using DfuseController::DfuseController;
};

}

#endif
