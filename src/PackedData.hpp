#ifndef PackedData_h
#define PackedData_h

#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <stdexcept>
#include <string>


// Classes for reading and writing packed integers in an array of bytes

namespace PackedData
{


class error_OutOfRange : public std::out_of_range
{
public:
	using std::out_of_range::out_of_range;
};

class Reader
{
protected:
	const uint8_t *data;
	size_t i;
	size_t length;
	[[noreturn]] void error_tooSmall();

public:
	Reader();
	Reader(const uint8_t *data_, size_t length_=SIZE_MAX);
	void reset();
	void skip(size_t count);
	bool enoughBytes(size_t count) const;
	size_t remainingBytes() const;
	bool eof() const;

	const uint8_t* getCurrPtr();

	uint8_t read_u8();
	int8_t read_i8() { return static_cast<int8_t>(read_u8()); }
	std::string read_string(size_t count);

	// Big endian (most significant byte first)
	uint32_t read_u32b();
	uint32_t read_u24b();
	uint16_t read_u16b();
	int32_t read_i32b() { return static_cast<int32_t>(read_u32b()); }
	int16_t read_i16b() { return static_cast<int16_t>(read_u16b()); }

	// Little endian (least significant byte first)
	uint32_t read_u32l();
	uint32_t read_u24l();
	uint16_t read_u16l();
	int32_t read_i32l() { return static_cast<int32_t>(read_u32l()); }
	int16_t read_i16l() { return static_cast<int16_t>(read_u16l()); }

	// Returns a Reader object for the next n bytes, and skips over them in this object
	Reader subReader(size_t n);
};




class Writer
{
protected:
	[[noreturn]] void error_tooSmall();

	uint8_t *data;
	size_t i;
	size_t length;
public:
	Writer(uint8_t *data_, size_t length_=SIZE_MAX);
	void skip(size_t count);
	bool enoughBytes(size_t count) const;
	void reset();

	void write_u8(uint8_t x);
	void write_i8(int8_t x) { write_u8(x); }

	/*// Big endian (most significant byte first)
	void write_u32b(uint32_t x);
	void write_u16b(uint16_t x);
	void write_i32b(int32_t x) { write_u32b(x); }
	void write_i16b(int16_t x) { write_u16b(x); }*/

	// Little endian (least significant byte first)
	void write_u32l(uint32_t x);
	void write_u16l(uint16_t x);
	void write_i32l(int32_t x) { write_u32l(x); }
	void write_i16l(int16_t x) { write_u16l(x); }

	// Returns a Writer object for the next n bytes, and skips over them in this object
	Writer subWriter(size_t n);
};

}


#endif
