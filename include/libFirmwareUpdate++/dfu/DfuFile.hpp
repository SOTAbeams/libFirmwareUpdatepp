#ifndef libFirmwareUpdate_dfu_DfuFile_h
#define libFirmwareUpdate_dfu_DfuFile_h

#include "libFirmwareUpdate++/Context.hpp"
#include "libFirmwareUpdate++/UsbId.hpp"

#include <cstdint>
#include <vector>
#include <string>
#include <iostream>

namespace FwUpd
{

class DfuFile
{
public:
	std::shared_ptr<Context> ctx;

	enum class PrefixType
	{
		None,
		LMDFU,
		LPCDFU_UNENCRYPTED,
	};

	// Entire file contents loaded into memory (including prefix and suffix)
	std::vector<uint8_t> data;

	// Sizes of various parts
	struct {
		uint32_t total;
		uint32_t prefix;
		uint32_t suffix;
		uint32_t getPayload() const
		{
			return total - prefix - suffix;
		}
	} size;

	// Data from prefix
	uint32_t lmdfu_address;
	PrefixType prefix_type;

	// Data from suffix
	uint32_t dwCRC;
	uint16_t bcdDFU;
	UsbId usbId;
	uint16_t bcdDevice;

	DfuFile(std::shared_ptr<Context> ctx);
	virtual ~DfuFile();

	void reset();
	void loadFile(std::string filename);
	void loadStdIn();

	void storeFile(std::string filename, bool writeSuffix, bool writePrefix);
	bool hasPrefix();
	bool hasSuffix();
	void printSuffixAndPrefix(std::ostream &stream = std::cout);
	void provideDefaultSearchId(UsbId *dst);

	UsbId getSearchId();
};

}

#endif
