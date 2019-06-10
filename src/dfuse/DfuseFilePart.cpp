#include "DfuseFilePart.hpp"
#include "ContextImpl.hpp"

namespace FwUpd
{
namespace DfuseFilePart
{

void Prefix::parse(ContextImpl *ctxi, PackedData::Reader d)
{
	signature = d.read_string(5);
	version = d.read_u8();
	imageSize = d.read_u32l();
	targetsCount = d.read_u8();

	if (signature!="DfuSe")
	{
		ctxi->logAndThrow(LogMsgType::FileFormatError, "No valid DfuSe signature");
	}
	if (version != 0x01)
	{
		ctxi->logfAndThrow(LogMsgType::FileFormatError, "DFU format revision %i not supported",
						   static_cast<int>(version));
	}
}

void TargetPrefix::parse(ContextImpl *ctxi, PackedData::Reader d)
{
	signature = d.read_string(6);
	alternateSetting = d.read_u8();
	targetNamed = d.read_u32l();
	targetName = d.read_string(255);
	targetSize = d.read_u32l();
	nbElements = d.read_u32l();

	if (signature!="Target")
	{
		ctxi->logAndThrow(LogMsgType::FileFormatError, "No valid target signature");
	}
}

void ElementHeader::parse(ContextImpl *ctxi, PackedData::Reader d)
{
	elementAddress = d.read_u32l();
	elementSize = d.read_u32l();
}

}
}
