#ifndef fwupd_CRC32_h
#define fwupd_CRC32_h

#include <cstdint>
#include <cstdlib>

namespace FwUpd
{

class CRC32
{
public:
	uint32_t val = 0xffffffff;

	void update_u8(uint8_t x);
	void update_u8(const uint8_t *x, size_t n);
	CRC32()
	{}
	CRC32(uint32_t val) : val(val)
	{}
	operator uint32_t()
	{
		return val;
	}
};

}


#endif
