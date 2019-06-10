#ifndef fwupd_dfuse_DfuseFilePart_h
#define fwupd_dfuse_DfuseFilePart_h

#include "PackedData.hpp"

#include <cstdint>

namespace FwUpd
{

class ContextImpl;

namespace DfuseFilePart
{

/* Note on endianness:
 * The file format spec from ST (UM0391) states for each of these parts of a DfuSe file that they are "represented in Big Endian order"
 * However, dfu-util and several other programs which interact with DfuSe files use little endian.
 * Even ST's DfuSe demo software appears to use struct packing and memcpy, which on x86 will result in little endian.
 * So UM0391 is wrong, these parts are little endian.
 */

class Prefix
{
public:
	static const int packedSize = 11;
	std::string signature;
	uint8_t version;
	uint32_t imageSize;
	uint8_t targetsCount;
	void parse(ContextImpl *ctxi, PackedData::Reader d);
};

class TargetPrefix
{
public:
	static const int packedSize = 274;
	std::string signature;
	uint8_t alternateSetting;
	bool targetNamed;
	std::string targetName;
	uint32_t targetSize;
	uint32_t nbElements;
	void parse(ContextImpl *ctxi, PackedData::Reader d);
};


class ElementHeader
{
public:
	static const int packedSize = 8;
	uint32_t elementAddress;
	uint32_t elementSize;
	void parse(ContextImpl *ctxi, PackedData::Reader d);
};

}

}

#endif
