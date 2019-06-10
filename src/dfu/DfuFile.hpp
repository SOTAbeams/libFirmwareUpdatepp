#ifndef fwupd_dfu_file_h
#define fwupd_dfu_file_h

#include "libFirmwareUpdate++/dfu.hpp"
#include "CRC32.hpp"
#include <cstdint>
#include <cstdlib>
#include <iosfwd>


namespace FwUpd
{

class ContextImpl;

// Class to parse the file data in a DfuFile object to fill the other member vars in the DfuFile (data from the prefix and suffix)
class DfuFileReader
{
protected:
	DfuFile *f;
	ContextImpl *ctxi();

	void logMissingSuffixReason(const char *reason);
	bool probePrefix();
	bool probeSuffix();

public:
	DfuFileReader(DfuFile *f);

	void read();
};

/*
 * Class to write the contents of a DfuFile object to a stream.
 *   Firmware data: always written
 *   Suffix: written if shouldWriteSuffix argument is true
 *   Prefix: written if f->prefix_type is a valid prefix type and shouldWritePrefix argument is true
 */
class DfuFileWriter
{
protected:
	DfuFile *f;
	std::ostream *dst = nullptr;
	CRC32 crc;

	void crcWrite(const uint8_t *data, size_t n);
	void writePrefix();
	void writeSuffix();

public:
	DfuFileWriter(DfuFile *f) : f(f)
	{}

	void write(std::ostream &dst_, bool shouldWriteSuffix, bool shouldWritePrefix);
};

}
#endif
