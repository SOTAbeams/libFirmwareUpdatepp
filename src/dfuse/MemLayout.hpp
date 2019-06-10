#ifndef fwupd_dfuse_mem_h
#define fwupd_dfuse_mem_h

#include <vector>
#include <cstdint>
#include <string>

namespace FwUpd
{

class ContextImpl;

namespace Dfuse
{

class MemSegment
{
public:
	enum
	{
		Flag_Readable=1,
		Flag_Eraseable=2,
		Flag_Writeable=4,
	} Flag;

	// Inclusive range of addresses
	uint32_t firstAddr, lastAddr;

	uint32_t pagesize;
	uint8_t memtype;

	bool isReadable() const
	{
		return (memtype & Flag_Readable);
	}
	bool isEraseable() const
	{
		return (memtype & Flag_Eraseable);
	}
	bool isWriteable() const
	{
		return (memtype & Flag_Writeable);
	}
};

class MemLayout
{
public:
	std::vector<MemSegment> segments;
	void clear();
	bool parseDesc(ContextImpl *ctxi, const std::string &intf_descStr);
	MemSegment *findSegment(uint32_t address);

	bool isAddressReadable(uint32_t address);
	bool isAddressEraseable(uint32_t address);
	bool isAddressWriteable(uint32_t address);
};

}
}

#endif
