#include "CRC32.hpp"
#include "PackedData.hpp"
#include "ContextImpl.hpp"
#include "Util.hpp"

#include "DfuFile.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <cinttypes>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>

#define DFU_SUFFIX_LENGTH 16
#define LMDFU_PREFIX_LENGTH 8
#define LPCDFU_PREFIX_LENGTH 16
#define PROGRESS_BAR_WIDTH 25
#define STDIN_CHUNK_SIZE 65536

namespace FwUpd
{

DfuFile::DfuFile(std::shared_ptr<Context> ctx) : ctx(ctx)
{

}

DfuFile::~DfuFile()
{

}

void DfuFile::reset()
{
	data.clear();
	size.total = 0;
	size.prefix = 0;
	size.suffix = 0;

	/* default values, if no valid prefix is found */
	lmdfu_address = 0;
	prefix_type = PrefixType::None;
	dwCRC = 0xFFFFFFFF;

	/* default values, if no valid suffix is found */
	bcdDFU = 0;
	// Wildcard values:
	usbId.clear();
	bcdDevice = 0xFFFF;
}

void DfuFile::loadFile(std::string filename)
{
	reset();

	std::ifstream f(filename, std::ios::binary | std::ios::ate);
	if (f.fail())
		ctx->pImpl->logAndThrow(LogMsgType::FileIoError, "Could not open file " + filename + " for reading");

	std::streampos fileSize = f.tellg();
	if (fileSize < 0 || (uint32_t)fileSize != fileSize)
		ctx->pImpl->logAndThrow(LogMsgType::FileIoError, "File size is too big");

	f.seekg(0, std::ios::beg);
	if (f.fail())
		ctx->pImpl->logAndThrow(LogMsgType::FileIoError, "Could not seek to beginning");

	data.resize(fileSize);
	f.read(reinterpret_cast<char*>(data.data()), fileSize);
	if (f.fail())
		ctx->pImpl->logAndThrow(LogMsgType::FileIoError, "Could not read from file");

	size.total = fileSize;
	DfuFileReader impl(this);
	impl.read();
}

void DfuFile::loadStdIn()
{
	reset();

#if defined(_WIN32) || defined(_WIN64)
	_setmode( _fileno( stdin ), _O_BINARY );
#endif
	size_t read_bytes;
	std::array<uint8_t, 65536> tmp;
	while ((read_bytes = std::fread(tmp.data(), sizeof(uint8_t), tmp.size(), stdin)))
	{
		size.total += read_bytes;
		data.reserve(size.total);
		std::copy(data.begin(), data.end(), std::back_inserter(data));
	}
	if (ctx->pImpl->shouldLog(LogLevel::Verbose))
		ctx->pImpl->logf(LogLevel::Verbose, "Read %" PRIu32 " bytes from stdin", size.total);

	DfuFileReader impl(this);
	impl.read();
}

void DfuFile::storeFile(std::string filename, bool writeSuffix, bool writePrefix)
{
	std::ofstream f(filename, std::ios::trunc | std::ios::binary);
	if (f.fail())
		throw Error(LogMsgType::FileIoError, "Could not open file " + filename + " for writing");

	DfuFileWriter impl(this);
	impl.write(f, writeSuffix, writePrefix);
	if (f.fail())
		throw Error(LogMsgType::FileIoError, "Could not write to file");
}

bool DfuFile::hasPrefix()
{
	return (size.prefix>0 && prefix_type!=PrefixType::None);
}

bool DfuFile::hasSuffix()
{
	return (size.suffix>0);
}

void DfuFile::printSuffixAndPrefix(std::ostream &s)
{
	if (size.prefix == LMDFU_PREFIX_LENGTH) {
		printfStream(s, "The file contains a TI Stellaris DFU prefix with the following properties:\n");
		printfStream(s, "Address:\t0x%08x\n", lmdfu_address);
	} else if (size.prefix == LPCDFU_PREFIX_LENGTH) {
		uint8_t * prefix = data.data();
		printfStream(s, "The file contains a NXP unencrypted LPC DFU prefix with the following properties:\n");
		printfStream(s, "Size:\t%5d kiB\n", prefix[2]>>1|prefix[3]<<7);
	} else if (size.prefix != 0) {
		printfStream(s, "The file contains an unknown prefix\n");
	}
	if (size.suffix > 0) {
		printfStream(s, "The file contains a DFU suffix with the following properties:\n");
		printfStream(s, "BCD device:\t0x%04X\n", bcdDevice);
		printfStream(s, "Product ID:\t0x%04X\n",usbId.product);
		printfStream(s, "Vendor ID:\t0x%04X\n", usbId.vendor);
		printfStream(s, "BCD DFU:\t0x%04X\n", bcdDFU);
		printfStream(s, "Length:\t\t%i\n", size.suffix);
		printfStream(s, "CRC:\t\t0x%08X\n", dwCRC);
	}
}

void DfuFile::provideDefaultSearchId(UsbId *dst)
{
	/* If dst does not have any vendor or product IDs to use for device matching,
	 * use any IDs from the file suffix for device matching.
	 *
	 * This has verbose logging. If verbose logging is not required,
	 * could use usbId.defaultsFrom(dfuFile.getSearchId()) instead.
	 */
	if (!hasSuffix() || !dst)
		return;
	UsbId src = getSearchId();
	if (!dst->hasVendor() && src.hasVendor())
	{
		dst->vendor = src.vendor;
		if (ctx->pImpl->shouldLog(LogLevel::Verbose))
			ctx->pImpl->logf(LogLevel::Verbose, "Match vendor ID from file: %04x", dst->vendor);
	}
	if (!dst->hasProduct() && src.hasProduct())
	{
		dst->product = src.product;
		if (ctx->pImpl->shouldLog(LogLevel::Verbose))
			ctx->pImpl->logf(LogLevel::Verbose, "Match product ID from file: %04x", dst->product);
	}
}

UsbId DfuFile::getSearchId()
{
	UsbId result;
	if (hasSuffix())
	{
		// TODO: DFU spec seems to indicate that vendor=0xFFFF means any device, even if product does not match?
		if (usbId.vendor!=0xFFFF)
			result.vendor = usbId.vendor;
		if (usbId.product!=0xFFFF)
			result.product = usbId.product;
	}
	return result;
}

ContextImpl *DfuFileReader::ctxi()
{
	return f->ctx->pImpl;
}

void DfuFileReader::logMissingSuffixReason(const char *reason)
{
	ctxi()->logf(LogLevel::Warn, "Missing suffix: %s", reason);
}

bool DfuFileReader::probePrefix()
{
	f->size.prefix = 0;
	uint8_t *prefix = f->data.data();

	size_t maxPrefixSize = f->size.total - f->size.suffix;
	if (LMDFU_PREFIX_LENGTH<=maxPrefixSize && (prefix[0] == 0x01) && (prefix[1] == 0x00))
	{
		f->prefix_type = DfuFile::PrefixType::LMDFU;
		f->size.prefix = LMDFU_PREFIX_LENGTH;
		f->lmdfu_address = 1024 * PackedData::Reader(prefix+2).read_u16l();
		return true;
	}
	else if (LPCDFU_PREFIX_LENGTH<=maxPrefixSize && ((prefix[0] & 0x3f) == 0x1a) && ((prefix[1] & 0x3f)== 0x3f))
	{
		f->prefix_type = DfuFile::PrefixType::LPCDFU_UNENCRYPTED;
		f->size.prefix = LPCDFU_PREFIX_LENGTH;
		return true;
	}
	return false;
}

bool DfuFileReader::probeSuffix()
{
	CRC32 crc;
	const uint8_t *dfusuffix;

	// TODO: only modify suffix vars if suffix was successfully parsed
	// TODO: update logging

	if (f->size.total < DFU_SUFFIX_LENGTH) {
		logMissingSuffixReason("File too short for DFU suffix");
		return false;
	}

	dfusuffix = f->data.data() + f->size.total -
		DFU_SUFFIX_LENGTH;


	// TODO: use memcmp, with DFU_SUFFIX_LENGTH-5-3 as origin
	if (dfusuffix[10] != 'D' ||
		dfusuffix[9]  != 'F' ||
		dfusuffix[8]  != 'U') {
		logMissingSuffixReason("Invalid DFU suffix signature");
		return false;
	}

	crc.update_u8(f->data.data(), f->size.total - 4);
	f->dwCRC = PackedData::Reader(dfusuffix+12).read_u32l();

	if (f->dwCRC != crc) {
		logMissingSuffixReason("DFU suffix CRC does not match");
		return false;
	}

	/* At this point we believe we have a DFU suffix
	   so we require further checks to succeed */

	f->bcdDFU = PackedData::Reader(dfusuffix+6).read_u16l();

	ctxi()->logf(LogLevel::Verbose, "DFU suffix version %x\n", f->bcdDFU);

	uint32_t suffixSize = dfusuffix[11];
	if (suffixSize < DFU_SUFFIX_LENGTH)
	{
		ctxi()->logfAndThrow(LogMsgType::FileFormatError, "Unsupported DFU suffix length " PRIu32, suffixSize);
	}

	if (suffixSize > f->size.total)
	{
		ctxi()->logfAndThrow(LogMsgType::FileFormatError, "Invalid DFU suffix length" PRIu32, suffixSize);
	}

	f->size.suffix = suffixSize;

	{
		PackedData::Reader d(dfusuffix);
		f->bcdDevice = d.read_u16l();
		f->usbId.product = d.read_u16l();
		f->usbId.vendor = d.read_u16l();
		f->bcdDFU = d.read_u16l();
	}
	return true;
}

DfuFileReader::DfuFileReader(DfuFile *f) : f(f)
{}

void DfuFileReader::read()
{
	probeSuffix();
	probePrefix();
}

void DfuFileWriter::crcWrite(const uint8_t *data, size_t n)
{
	crc.update_u8(data, n);
	dst->write(reinterpret_cast<const char*>(data), n);
	if (!dst->good())
		throw Error(LogMsgType::FileIoError, "Could not write to file");
}

void DfuFileWriter::writePrefix()
{
	if (f->prefix_type == DfuFile::PrefixType::LMDFU) {
		uint8_t lmdfu_prefix[LMDFU_PREFIX_LENGTH];
		uint32_t addr = f->lmdfu_address / 1024;

		/* lmdfu_dfu_prefix payload length excludes prefix and suffix */
		uint32_t len = f->size.total -
			f->size.prefix - f->size.suffix;

		PackedData::Writer d(lmdfu_prefix, LMDFU_PREFIX_LENGTH);
		d.write_u8(0x01); /* STELLARIS_DFU_PROG */
		d.write_u8(0x00); /* Reserved */
		d.write_u16l(addr);
		d.write_u32l(len);

		crcWrite(lmdfu_prefix, LMDFU_PREFIX_LENGTH);
	}
	if (f->prefix_type == DfuFile::PrefixType::LPCDFU_UNENCRYPTED) {
		uint8_t lpcdfu_prefix[LPCDFU_PREFIX_LENGTH] = {0};

		/* Payload is firmware and prefix rounded to 512 bytes */
		uint32_t len = (f->size.total - f->size.suffix + 511) /512;

		PackedData::Writer d(lpcdfu_prefix, LPCDFU_PREFIX_LENGTH);
		d.write_u8(0x1a); /* Unencrypted*/
		d.write_u8(0x3f); /* Reserved */
		d.write_u16l(len);

		for (int i = 12; i < LPCDFU_PREFIX_LENGTH; i++)
			lpcdfu_prefix[i] = 0xff;

		crcWrite(lpcdfu_prefix, LPCDFU_PREFIX_LENGTH);
	}
}

void DfuFileWriter::writeSuffix()
{
	uint8_t dfusuffix[DFU_SUFFIX_LENGTH];

	PackedData::Writer d(dfusuffix, DFU_SUFFIX_LENGTH);
	d.write_u16l(f->bcdDevice);
	d.write_u16l(f->usbId.product);
	d.write_u16l(f->usbId.vendor);
	d.write_u16l(f->bcdDFU);
	d.write_u8('U');
	d.write_u8('F');
	d.write_u8('D');
	d.write_u8(DFU_SUFFIX_LENGTH);
	crcWrite(dfusuffix, DFU_SUFFIX_LENGTH - 4);

	d.write_u32l(crc);
	crcWrite(dfusuffix + 12, 4);
}

void DfuFileWriter::write(std::ostream &dst_, bool shouldWriteSuffix, bool shouldWritePrefix)
{
	dst = &dst_;
	crc = 0xFFFFFFFF;

	if (shouldWritePrefix) {
		writePrefix();
	}

	/* write firmware binary */
	crcWrite(f->data.data() + f->size.prefix,
	    f->size.total - f->size.prefix - f->size.suffix);

	if (shouldWriteSuffix) {
		writeSuffix();
	}
}

}
