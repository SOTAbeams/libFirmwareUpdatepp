#include "DfuController.hpp"
#include "ContextImpl.hpp"
#include "usb_dfu.hpp"
#include "Util.hpp"

#ifdef HAVE_GETPAGESIZE
#include <unistd.h>
#endif

namespace FwUpd
{

ContextImpl *DfuController::ctxi() const
{
	return dif->ctx->pImpl;
}

uint32_t DfuController::getDefaultTransferSize()
{
	if (!dif)
		return 0;
	uint32_t x = dif->func_dfu.wTransferSize;
	if (x) {
		ctxi()->logf(LogLevel::Info, "Device returned transfer size %i", x);
	} else {
		ctxi()->logAndThrow("Transfer size must be specified");
	}

#if defined(HAVE_GETPAGESIZE) && !(defined(__MINGW32__) || defined(_WIN32) || defined(_WIN64))
	/* limitation of Linux usbdevio */
	if ((int)x > getpagesize())
	{
		x = getpagesize();
		ctxi()->logf(LogLevel::Info, "Limited transfer size to %i", x);
	}
#endif

	if (x < dif->bMaxPacketSize0) {
		x = dif->bMaxPacketSize0;
		ctxi()->logf(LogLevel::Info, "Adjusted transfer size to %i", x);
	}

	return x;
}

void DfuController::calcTransferSize()
{
	if (transferSizeOverride)
		transferSize = transferSizeOverride;
	transferSize = getDefaultTransferSize();
}

void DfuController::abortToIdle()
{
	struct dfu_status dst;

	ctxi()->assert_usbXferOk(dif->abort(), "Error sending dfu abort request");
	ctxi()->assert_usbXferOk(dif->getStatus(&dst), "Error during abort get_status");
	if (dst.bState != DFU_STATE_dfuIDLE) {
		ctxi()->logAndThrow("Failed to enter idle state on abort");
	}
	milliSleep(dst.bwPollTimeout);
}

DfuController::DfuController(std::shared_ptr<DfuInterface> dif) :
	dif(dif)
{}

int DfuController_download::run()
{
	calcTransferSize();

	int bytes_sent;
	int expected_size;
	unsigned short transaction = 0;
	struct dfu_status dst;
	int ret;

	ctxi()->log(LogLevel::Info, "Copying data from PC to "+ctxi()->getProductName());

	// Note: sent data includes prefix (if any)
	uint8_t *buf = file->data.data();
	expected_size = file->size.total - file->size.suffix;
	bytes_sent = 0;

	ctxi()->progress(0.05, "Downloading");
	while (bytes_sent < expected_size) {
		int bytes_left;
		int chunk_size;

		bytes_left = expected_size - bytes_sent;
		if (bytes_left < transferSize)
			chunk_size = bytes_left;
		else
			chunk_size = transferSize;

		ctxi()->assert_usbXferOk(dif->download(transaction++, chunk_size ? buf : nullptr, chunk_size),
								 "Error during download");
		bytes_sent += chunk_size;
		buf += chunk_size;

		do {
			ctxi()->assert_usbXferOk(dif->getStatus(&dst),
									 "Error during download get_status");

			if (dst.bState == DFU_STATE_dfuDNLOAD_IDLE ||
					dst.bState == DFU_STATE_dfuERROR)
				break;

			/* Wait while device executes flashing */
			milliSleep(dst.bwPollTimeout);

		} while (1);
		if (dst.bStatus != DFU_STATUS_OK) {
			ctxi()->logfAndThrow("Error during download: state(%u) = %s, status(%u) = %s", dst.bState,
				dfu_state_to_string(dst.bState), dst.bStatus,
				dfu_status_to_string(dst.bStatus));
		}
		// <10% and >90% reserved for enumeration/reset/other programming tasks
		float prog = (static_cast<float>(bytes_sent)/(bytes_sent + bytes_left)) * 0.9 + 0.05;
		ctxi()->progress(prog, "Downloading");
	}

	/* send one zero sized download request to signalize end */
	ctxi()->assert_usbXferOk(dif->download(transaction, nullptr, 0),
							 "Error sending completion packet");

	ctxi()->progress(0.95, "Downloading");

	ctxi()->logf(LogLevel::Verbose, "Sent a total of %i bytes", bytes_sent);

get_status:
	/* Transition to MANIFEST_SYNC state */
	ret = dif->getStatus(&dst);
	if (ret < 0) {
		ctxi()->logf(LogLevel::Warn, "unable to read DFU status after completion");
		return bytes_sent;
	}
	ctxi()->logf(LogLevel::Info, "state(%u) = %s, status(%u) = %s\n", dst.bState,
		dfu_state_to_string(dst.bState), dst.bStatus,
		dfu_status_to_string(dst.bStatus));

	milliSleep(dst.bwPollTimeout);

	/* FIXME: deal correctly with ManifestationTolerant=0 / WillDetach bits */
	switch (dst.bState) {
	case DFU_STATE_dfuMANIFEST_SYNC:
	case DFU_STATE_dfuMANIFEST:
		/* some devices (e.g. TAS1020b) need some time before we
		 * can obtain the status */
		milliSleep(1000);
		goto get_status;
		break;
	case DFU_STATE_dfuIDLE:
		break;
	}
	ctxi()->logf(LogLevel::Info, "Done!");

	return bytes_sent;
}

}
