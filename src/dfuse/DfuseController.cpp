#include "DfuseController.hpp"
#include "ContextImpl.hpp"
#include "dfu/usb_dfu.hpp"
#include "PackedData.hpp"
#include "DfuseFilePart.hpp"
#include "MemLayout.hpp"
#include "Util.hpp"

namespace FwUpd
{

const char* DfuseCommand_toString(DfuseCommand cmd)
{
	switch (cmd)
	{
	case DfuseCommand::SetAddress:
		return "SET_ADDRESS";
	case DfuseCommand::ErasePage:
		return "ERASE_PAGE";
	case DfuseCommand::MassErase:
		return "MASS_ERASE";
	case DfuseCommand::ReadUnprotect:
		return "READ_UNPROTECT";
	default:
		return nullptr;
	}
}

int DfuseController::specialCommand(unsigned int address, DfuseCommand command)
{
	unsigned char buf[5];
	PackedData::Writer bufW(buf, sizeof(buf));
	int length;
	int ret;
	struct dfu_status dst;
	int firstpoll = 1;

	if (command == DfuseCommand::ErasePage) {
		int page_size;

		Dfuse::MemSegment *segment = memLayout.findSegment(address);
		if (!segment || !segment->isEraseable()) {
			ctxi()->logfAndThrow("Page at 0x%08x can not be erased",
				address);
		}
		page_size = segment->pagesize;
		ctxi()->logf(LogLevel::Verbose2, "Erasing page size %i at address 0x%08x, page "
			       "starting at 0x%08x\n", page_size, address,
			       address & ~(page_size - 1));
		bufW.write_u8(0x41);/* Erase command */
		length = 5;
		last_erased_page = address & ~(page_size - 1);
	} else if (command == DfuseCommand::SetAddress) {
		ctxi()->logf(LogLevel::Verbose3, "Setting address pointer to 0x%08x\n",
			       address);
		bufW.write_u8(0x21);	/* Set Address Pointer command */
		length = 5;
	} else if (command == DfuseCommand::MassErase) {
		bufW.write_u8(0x41);	/* Mass erase command when length = 1 */
		length = 1;
	} else if (command == DfuseCommand::ReadUnprotect) {
		bufW.write_u8(0x92);
		length = 1;
	} else {
		ctxi()->logfAndThrow("Non-supported special command %d", command);
	}
	bufW.write_u32l(address);

	ret = req_dnload(length, buf, 0);
	if (ret < 0) {
		ctxi()->logfAndThrow("Error during special command \"%s\" download",
			DfuseCommand_toString(command));
	}
	do {
		ret = dif->getStatus(&dst);
		if (ret < 0) {
			ctxi()->logfAndThrow("Error during special command \"%s\" get_status",
			     DfuseCommand_toString(command));
		}
		if (firstpoll) {
			firstpoll = 0;
			if (dst.bState != DFU_STATE_dfuDNBUSY) {
				ctxi()->logf(LogLevel::Info, "state(%u) = %s, status(%u) = %s", dst.bState,
				       dfu_state_to_string(dst.bState), dst.bStatus,
				       dfu_status_to_string(dst.bStatus));
				ctxi()->logfAndThrow("Wrong state after command \"%s\" download",
				     DfuseCommand_toString(command));
			}
			/* STM32F405 lies about mass erase timeout */
			if (command == DfuseCommand::MassErase && dst.bwPollTimeout == 100) {
				dst.bwPollTimeout = 35000;
				ctxi()->logf(LogLevel::Info, "Setting timeout to 35 seconds\n");
			}
		}
		/* wait while command is executed */
		ctxi()->logf(LogLevel::Verbose, "Poll timeout %i ms", dst.bwPollTimeout);
		milliSleep(dst.bwPollTimeout);
		if (command == DfuseCommand::ReadUnprotect)
			return ret;
	} while (dst.bState == DFU_STATE_dfuDNBUSY);

	if (dst.bStatus != DFU_STATUS_OK) {
		ctxi()->logfAndThrow("%s not correctly executed",
			DfuseCommand_toString(command));
	}
	return ret;
}

int DfuseController::req_upload(const unsigned short length, unsigned char *data, unsigned short transaction)
{
	int status = dif->upload(transaction, data, length);
	if (status < 0) {
		ctxi()->logfAndThrow(LogMsgType::UsbIoError, "%s: libusb_control_transfer returned %d",
			__FUNCTION__, status);
	}
	return status;
}

int DfuseController::req_dnload(const unsigned short length, unsigned char *data, unsigned short transaction)
{
	int status = dif->download(transaction, data, length);
	if (status < 0) {
		ctxi()->logfAndThrow(LogMsgType::UsbIoError, "%s: libusb_control_transfer returned %d",
			__FUNCTION__, status);
	}
	return status;
}



void DfuseController_download::progress(const uint8_t *dataPos)
{
	// Progress is indicated by read position in the file
	// It is assumed that dataPos points to somewhere in file->data, not a copy of the data.
	size_t i = dataPos - (file->data.data()+file->size.prefix);
	// <10% and >90% reserved for enumeration/reset/other programming tasks
	float prog = (static_cast<float>(i)/file->size.getPayload()) * 0.9 + 0.05;
	ctxi()->progress(prog, "Downloading");
}

int DfuseController_download::dnload_chunk(const uint8_t *data, int size, int transaction)
{
	int bytes_sent;
	struct dfu_status dst;

	bytes_sent = req_dnload(size, size ? const_cast<uint8_t*>(data) : NULL, transaction);

	do {
		ctxi()->assert_usbXferOk(dif->getStatus(&dst), "Error during download get_status");
		milliSleep(dst.bwPollTimeout);
	} while (dst.bState != DFU_STATE_dfuDNLOAD_IDLE &&
			 dst.bState != DFU_STATE_dfuERROR &&
		 dst.bState != DFU_STATE_dfuMANIFEST);

	if (dst.bState == DFU_STATE_dfuMANIFEST)
		ctxi()->log(LogLevel::Info, "Transitioning to dfuMANIFEST state");

	if (dst.bStatus != DFU_STATUS_OK) {
		ctxi()->logf(LogLevel::Warn, "Chunk write failed! state(%u) = %s, status(%u) = %s", dst.bState,
		       dfu_state_to_string(dst.bState), dst.bStatus,
		       dfu_status_to_string(dst.bStatus));
		return -1;
	}
	return bytes_sent;
}

/* Writes an element of any size to the device, taking care of page erases */
/* returns 0 on success, otherwise -EINVAL */
int DfuseController_download::dnload_element(unsigned int dwElementAddress,
												unsigned int dwElementSize, const uint8_t *data)
{
	int p;
	int ret;

	/* Check at least that we can write to the last address */
	if (!memLayout.isAddressWriteable(dwElementAddress + dwElementSize - 1)) {
		ctxi()->logfAndThrow("Last page at 0x%08x is not writeable",
			dwElementAddress + dwElementSize - 1);
	}


	for (p = 0; p < (int)dwElementSize; p += transferSize) {
		int page_size;
		unsigned int erase_address;
		unsigned int address = dwElementAddress + p;
		int chunk_size = transferSize;

		Dfuse::MemSegment *segment = memLayout.findSegment(address);
		if (!segment || !segment->isWriteable()) {
			ctxi()->logfAndThrow("Page at 0x%08x is not writeable",
				address);
		}
		page_size = segment->pagesize;

		/* check if this is the last chunk */
		if (p + chunk_size > (int)dwElementSize)
			chunk_size = dwElementSize - p;

		/* Erase only for flash memory downloads */
		if (segment->isEraseable() && !opts->massErase) {
			/* erase all involved pages */
			for (erase_address = address;
			     erase_address < address + chunk_size;
			     erase_address += page_size)
				if ((erase_address & ~(page_size - 1)) !=
				    last_erased_page)
					specialCommand(erase_address,
							      DfuseCommand::ErasePage);

			if (((address + chunk_size - 1) & ~(page_size - 1)) !=
			    last_erased_page) {
				ctxi()->log(LogLevel::Verbose3, "Chunk extends into next page, erase it as well");
				specialCommand(address + chunk_size - 1,
						      DfuseCommand::ErasePage);
			}
		}

		ctxi()->logf(LogLevel::Verbose, " Download from image offset "
			       "%08x to memory %08x-%08x, size %i\n",
			       p, address, address + chunk_size - 1,
			       chunk_size);

		progress(data+p);
		specialCommand(address, DfuseCommand::SetAddress);

		/* transaction = 2 for no address offset */
		ret = dnload_chunk(data + p, chunk_size, 2);
		if (ret != chunk_size) {
			ctxi()->logfAndThrow("Failed to write whole chunk: "
				"%i of %i bytes", ret, chunk_size);
		}
	}
	progress(data+dwElementSize);
	return 0;
}

int DfuseController_download::dnload_dfuseFile()
{
	DfuseFilePart::Prefix dfuPrefix;
	DfuseFilePart::TargetPrefix targetPrefix;
	DfuseFilePart::ElementHeader elementHeader;

	ctxi()->progress(0.05, "Downloading");

	int ret;
	int bFirstAddressSaved = 0;

	PackedData::Reader data(file->data.data() + file->size.prefix, file->size.getPayload());

        /* Must be larger than a minimal DfuSe header and suffix */
	if (!data.enoughBytes(dfuPrefix.packedSize+targetPrefix.packedSize+elementHeader.packedSize))
	{
		ctxi()->logAndThrow(LogMsgType::FileFormatError, "File too small for a DfuSe file");
	}

	dfuPrefix.parse(ctxi(), data.subReader(dfuPrefix.packedSize));
	ctxi()->logf(LogLevel::Info, "file contains %i DFU images", dfuPrefix.targetsCount);

	for (uint8_t image = 1; image <= dfuPrefix.targetsCount; image++) {
		ctxi()->logf(LogLevel::Info, "parsing DFU image %i", static_cast<int>(image));
		targetPrefix.parse(ctxi(), data.subReader(targetPrefix.packedSize));

		ctxi()->logf(LogLevel::Info, "image for alternate setting %i, (%i elements, total size = %i)",
					 static_cast<int>(targetPrefix.alternateSetting),
					 static_cast<int>(targetPrefix.nbElements),
					 static_cast<int>(targetPrefix.targetSize));
		if (targetPrefix.alternateSetting != dif->altsetting)
			ctxi()->log(LogLevel::Warn, "Image does not match current alternate setting.\n"
			       "Please rerun with the correct -a option setting to download this image!");
		for (uint32_t element = 1; element <= targetPrefix.nbElements; element++) {
			ctxi()->logf(LogLevel::Info, "parsing element %i, ", static_cast<int>(element));
			elementHeader.parse(ctxi(), data.subReader(elementHeader.packedSize));

			ctxi()->logf(LogLevel::Info, "address = 0x%08x, size = %i", elementHeader.elementAddress, elementHeader.elementSize);

			if (!bFirstAddressSaved) {
				bFirstAddressSaved = 1;
				dfuse_address = elementHeader.elementAddress;
			}
			/* sanity check */
			if (!data.enoughBytes(elementHeader.elementSize))
				ctxi()->logfAndThrow(LogMsgType::FileFormatError, "File too small for element size");

			if (targetPrefix.alternateSetting == dif->altsetting) {
				ret = dnload_element(elementHeader.elementAddress, elementHeader.elementSize, data.getCurrPtr());
			} else {
				ret = 0;
			}

			/* advance read pointer */
			data.skip(elementHeader.elementSize);

			// TODO: check whther return value check is needed, or whether all errors are now handled by exceptions
			if (ret != 0)
				return ret;
		}
	}

	if (data.remainingBytes()!=0)
		ctxi()->logf(LogLevel::Warn, "%d bytes leftover", data.remainingBytes());

	ctxi()->log(LogLevel::Info, "done parsing DfuSe file");

	return 0;
}

int DfuseController_download::run()
{
	calcTransferSize();
	last_erased_page = 1; /* non-aligned value, won't match */

	int ret;

	if (!memLayout.parseDesc(ctxi(), dif->alt_name)) {
		ctxi()->logAndThrow(LogMsgType::UsbIoError, "Failed to parse memory layout");
	}
	if (opts->unprotect) {
		if (!opts->force) {
			ctxi()->logAndThrow(LogMsgType::InvalidOptions, "The read unprotect command "
				"will erase the flash memory"
				"and can only be used with force");
		}
		specialCommand(0, DfuseCommand::ReadUnprotect);
		ctxi()->log(LogLevel::Info, "Device disconnects, erases flash and resets now");
		return 0;
	}
	if (opts->massErase) {
		if (!opts->force) {
			ctxi()->logAndThrow(LogMsgType::InvalidOptions, "The mass erase command "
				"can only be used with force");
		}
		ctxi()->progress(0.1, "Mass erase");
		ctxi()->log(LogLevel::Info, "Performing mass erase, this can take a moment");
		specialCommand(0, DfuseCommand::MassErase);
	}

	if (file->bcdDFU != 0x11a) {
		ctxi()->logAndThrow("Only DfuSe file version 1.1a is supported for DfuSe format files");
	}
	ret = dnload_dfuseFile();
	memLayout.clear();

	abortToIdle();

	if (opts->leave) {
		specialCommand(dfuse_address, DfuseCommand::SetAddress);
		dnload_chunk(nullptr, 0, 2); /* Zero-size */
	}
	return ret;
}


}
