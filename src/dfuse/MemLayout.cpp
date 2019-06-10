/*
 * Helper functions for reading the memory map of a device
 * following the ST DfuSe 1.1a specification.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "dfu/DfuFile.hpp"
#include "dfuse/MemLayout.hpp"
#include "ContextImpl.hpp"

namespace FwUpd
{
namespace Dfuse
{

void MemLayout::clear()
{
	segments.resize(0);
}

bool MemLayout::parseDesc(ContextImpl *ctxi, const std::string& intf_descStr)
{
	clear();

	const char *intf_desc = intf_descStr.c_str();
	char multiplier, memtype;
	unsigned int address;
	int sectors, size;
	char *name, *typestring;
	int ret;
	int count = 0;
	char separator;
	int scanned;
	MemSegment segment;

	name = (char*)malloc(intf_descStr.length());

	ret = sscanf(intf_desc, "@%[^/]%n", name, &scanned);
	if (ret < 1) {
		free(name);
		ctxi->logf(LogLevel::Warn, "Could not read name, sscanf returned %d", ret);
		return false;
	}
	ctxi->logf(LogLevel::Info, "DfuSe interface name: \"%s\"", name);

	intf_desc += scanned;
	typestring = (char*)malloc(strlen(intf_desc));

	while (ret = sscanf(intf_desc, "/0x%x/%n", &address, &scanned),
	       ret > 0) {

		intf_desc += scanned;
		while (ret = sscanf(intf_desc, "%u*%u%c%[^,/]%n",
				    &sectors, &size, &multiplier, typestring,
				    &scanned), ret > 2) {
			intf_desc += scanned;

			count++;
			memtype = 0;
			if (ret == 4) {
				if (strlen(typestring) == 1
				    && typestring[0] != '/')
					memtype = typestring[0];
				else {
					ctxi->logf(LogLevel::Warn, "Parsing type identifier '%s' "
						"failed for segment %i",
						typestring, count);
					continue;
				}
			}

			/* Quirk for STM32F4 devices */
			if (strcmp(name, "Device Feature") == 0)
				memtype = 'e';

			switch (multiplier) {
			case 'B':
				break;
			case 'K':
				size *= 1024;
				break;
			case 'M':
				size *= 1024 * 1024;
				break;
			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
			case 'g':
				if (!memtype) {
					ctxi->logf(LogLevel::Warn, "Non-valid multiplier '%c', "
						"interpreted as type "
						"identifier instead",
						multiplier);
					memtype = multiplier;
					break;
				}
				/* fallthrough if memtype was already set */
			default:
				ctxi->logf(LogLevel::Warn, "Non-valid multiplier '%c', "
					"assuming bytes", multiplier);
			}

			if (!memtype) {
				ctxi->logf(LogLevel::Warn, "No valid type for segment %d\n", count);
				continue;
			}

			segment.firstAddr = address;
			segment.lastAddr = address + sectors * size - 1;
			segment.pagesize = size;
			segment.memtype = memtype & 7;
			segments.push_back(segment);

			ctxi->logf(LogLevel::Verbose, "Memory segment at 0x%08x %3d x %4d = "
				       "%5d (%s%s%s)\n",
				       address, sectors, size, sectors * size,
				       segment.isReadable()  ? "r" : "",
				       segment.isEraseable()  ? "e" : "",
				       segment.isWriteable() ? "w" : "");

			address += sectors * size;

			separator = *intf_desc;
			if (separator == ',')
				intf_desc += 1;
			else
				break;
		}	/* while per segment */

	}		/* while per address */
	free(name);
	free(typestring);
	return true;
}

MemSegment *MemLayout::findSegment(uint32_t address)
{
	for (MemSegment &seg : segments)
	{
		if (seg.firstAddr<=address && seg.lastAddr>=address)
			return &seg;
	}
	return nullptr;
}

bool MemLayout::isAddressReadable(uint32_t address)
{
	MemSegment *segment = findSegment(address);
	return (segment && segment->isReadable());
}

bool MemLayout::isAddressEraseable(uint32_t address)
{
	MemSegment *segment = findSegment(address);
	return (segment && segment->isEraseable());
}

bool MemLayout::isAddressWriteable(uint32_t address)
{
	MemSegment *segment = findSegment(address);
	return (segment && segment->isWriteable());
}

}
}
