#ifndef libFirmwareUpdate_dfu_DfuseOptions_h
#define libFirmwareUpdate_dfu_DfuseOptions_h

#include <cstdint>

namespace FwUpd
{

class DfuseOptions
{
public:
	bool force = false;
	bool leave = false;
	bool unprotect = false;
	bool massErase = false;
};

}

#endif
