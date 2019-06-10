#include "PackedData.hpp"

namespace PackedData
{

void Reader::error_tooSmall()
{
	throw error_OutOfRange("PackedData::Reader : length of data exceeded");
}

Reader::Reader() :
	data(nullptr), length(0)
{
	reset();
}

Reader::Reader(const uint8_t *data_, size_t length_) :
	data(data_), length(length_)
{
	reset();
}


void Reader::reset()
{
	i = 0;
}

void Reader::skip(size_t count)
{
	if (enoughBytes(count))
		i += count;
	else
		error_tooSmall();
}

bool Reader::enoughBytes(size_t count) const
{
	// Returns true if able to read the specified number of additional bytes
	// (assumes no integer overflow)
	return (i+count<=length);
}

size_t Reader::remainingBytes() const
{
	return (length-i);
}

bool Reader::eof() const
{
	return (i>=length);
}

const uint8_t *Reader::getCurrPtr()
{
	return data+i;
}

uint8_t Reader::read_u8()
{
	if (i>=length)
		error_tooSmall();
	return data[i++];
}

std::string Reader::read_string(size_t count)
{
	std::string x;
	x.reserve(count+1);
	for (size_t j=0; j<count; j++)
		x.push_back(read_i8());
	return x;
}

uint32_t Reader::read_u32b()
{
	using T = uint32_t;
	using FT = uint_fast32_t;
	if (!enoughBytes(sizeof(T)))
		error_tooSmall();
	const uint8_t *p = data+i;
	i += sizeof(T);
	return FT(p[3]) | (FT(p[2])<<8) | (FT(p[1])<<16) | (FT(p[0])<<24);
}

uint32_t Reader::read_u24b()
{
	const int byteCount = 3;
	using FT = uint_fast32_t;
	if (!enoughBytes(byteCount))
		error_tooSmall();
	const uint8_t *p = data+i;
	i += byteCount;
	return FT(p[2]) | (FT(p[1])<<8) | (FT(p[0])<<16);
}

uint16_t Reader::read_u16b()
{
	using T = uint16_t;
	using FT = uint_fast16_t;
	if (!enoughBytes(sizeof(T)))
		error_tooSmall();
	const uint8_t *p = data+i;
	i += sizeof(T);
	return FT(p[1]) | (FT(p[0])<<8);
}

uint32_t Reader::read_u32l()
{
	using T = uint32_t;
	using FT = uint_fast32_t;
	if (!enoughBytes(sizeof(T)))
		error_tooSmall();
	const uint8_t *p = data+i;
	i += sizeof(T);
	return FT(p[0]) | (FT(p[1])<<8) | (FT(p[2])<<16) | (FT(p[3])<<24);
}

uint32_t Reader::read_u24l()
{
	const int byteCount = 3;
	using FT = uint_fast32_t;
	if (!enoughBytes(byteCount))
		error_tooSmall();
	const uint8_t *p = data+i;
	i += byteCount;
	return FT(p[0]) | (FT(p[1])<<8) | (FT(p[2])<<16);
}

uint16_t Reader::read_u16l()
{
	using T = uint16_t;
	using FT = uint_fast16_t;
	if (!enoughBytes(sizeof(T)))
		error_tooSmall();
	const uint8_t *p = data+i;
	i += sizeof(T);
	return FT(p[0]) | (FT(p[1])<<8);
}

Reader Reader::subReader(size_t n)
{
	Reader x(data+i, n);
	skip(n);
	return x;
}


void Writer::error_tooSmall()
{
	throw error_OutOfRange("PackedData::Writer : length of data exceeded");
}

Writer::Writer(uint8_t *data_, size_t length_) : data(data_), length(length_)
{
	reset();
}

void Writer::skip(size_t count)
{
	if (enoughBytes(count))
		i += count;
	else
		error_tooSmall();
}

bool Writer::enoughBytes(size_t count) const
{
	return (i+count<=length);
}

void Writer::reset()
{
	i = 0;
}

void Writer::write_u8(uint8_t x)
{
	if (i<length)
		data[i++] = x;
	else
		error_tooSmall();
}

void Writer::write_u32l(uint32_t x)
{
	for (int i=0; i<4; i++)
	{
		write_u8((x>>(i*8)) & 0xFF);
	}
}

void Writer::write_u16l(uint16_t x)
{
	write_u8(x & 0xFF);
	write_u8((x>>8) & 0xFF);
}

Writer Writer::subWriter(size_t n)
{
	Writer x(data+i, n);
	skip(n);
	return x;
}

}
