#include "libFirmwareUpdate++/dfu/DfuDownloader.hpp"
#include "ContextImpl.hpp"
#include "dfu/usb_dfu.hpp"
#include "Util.hpp"
#include "dfu/DfuController.hpp"
#include "dfuse/DfuseController.hpp"
#include <libusb.h>

namespace FwUpd
{

bool DfuDownloader::run()
{
	try {
		file->provideDefaultSearchId(&probe.match_usbId);

		struct dfu_status status;

		int ret;
		int dfuse_device = 0;
		int detach_delay = 5;
		UsbId runtime_usbId;

		std::shared_ptr<FwUpd::DfuInterface> dif;

		ctx->pImpl->progress(0, "Searching USB devices");

		probe.matchDfuOnly = false;
		FwUpd::DfuFinder::Results dfuDevices = probe.find();
		if (!dfuDevices.size()) {
			ctx->pImpl->logAndThrow(LogMsgType::MatchError_NoMatches, "No matching DFU capable USB device found");
		} else if (dfuDevices.size()>1) {
			/* We cannot safely support more than one DFU capable device
			 * with same vendor/product ID, since during DFU we need to do
			 * a USB bus reset, after which the target device will get a
			 * new address */
			ctx->pImpl->logAndThrow(LogMsgType::MatchError_TooManyMatches, "More than one matching DFU capable USB device found! Try disconnecting all but one device");
		}


		/* We have exactly one device. */
		dif = dfuDevices[0];

		ctx->pImpl->log(LogLevel::Info, "Opening "+ctx->pImpl->getProductName());
		dif->openDevice();

		ctx->pImpl->logf(LogLevel::Info, "ID %04x:%04x", dif->usbId.vendor, dif->usbId.product);

		ctx->pImpl->logf(LogLevel::Info, "Run-time device DFU version %04x",
			   dif->func_dfu.bcdDFUVersion);

		/* Transition from run-Time mode to DFU mode */
		if (!(dif->flags & DFU_IFF_DFU)) {
			int err;
			/* In the 'first round' during runtime mode, there can only be one
			* DFU Interface descriptor according to the DFU Spec. */

			/* FIXME: check if the selected device really has only one */

			runtime_usbId = dif->usbId;

			ctx->pImpl->log(LogLevel::Info, "Claiming USB DFU Runtime Interface...");
			dif->claimInterface();

			if (libusb_set_interface_alt_setting(dif->dev_handle, dif->interface, 0) < 0) {
				ctx->pImpl->logAndThrow("Cannot set alt interface zero");
			}

			ctx->pImpl->log(LogLevel::Info, "Determining device status: ");

			err = dif->getStatus(&status);
			if (err == LIBUSB_ERROR_PIPE) {
				ctx->pImpl->log(LogLevel::Info, "Device does not implement get_status, assuming appIDLE");
				status.bStatus = DFU_STATUS_OK;
				status.bwPollTimeout = 0;
				status.bState  = DFU_STATE_appIDLE;
				status.iString = 0;
			} else if (err < 0) {
				ctx->pImpl->logAndThrow("error get_status");
			} else {
				ctx->pImpl->logf(LogLevel::Info, "state = %s, status = %d\n",
					   dfu_state_to_string(status.bState), status.bStatus);
			}
			milliSleep(status.bwPollTimeout);

			switch (status.bState) {
			case DFU_STATE_appIDLE:
			case DFU_STATE_appDETACH:
				ctx->pImpl->logf(LogLevel::Info, "Device really in Runtime Mode, sending DFU "
					   "detach request...");
				if (dif->detach(1000) < 0) {
					ctx->pImpl->log(LogLevel::Warn, "error detaching");
				}
				if (dif->func_dfu.attr_willDetach()) {
					ctx->pImpl->log(LogLevel::Info, "Device will detach and reattach...");
				} else {
					ctx->pImpl->log(LogLevel::Info, "Resetting USB...");
					ret = libusb_reset_device(dif->dev_handle);
					if (ret < 0 && ret != LIBUSB_ERROR_NOT_FOUND)
						ctx->pImpl->logAndThrow("error resetting "
							"after detach");
				}
				break;
			case DFU_STATE_dfuERROR:
				ctx->pImpl->log(LogLevel::Info, "dfuERROR, clearing status");
				if (dif->clearStatus() < 0) {
					ctx->pImpl->logAndThrow("error clear_status");
				}
				/* fall through */
			default:
				ctx->pImpl->log(LogLevel::Warn, "WARNING: Runtime device already in DFU state ?!?");
				dif->releaseInterface();
				goto dfustate;
			}
			dif->releaseInterface();
			dif->closeDevice();

			/* keeping handles open might prevent re-enumeration */
			dif = nullptr;
			dfuDevices.clear();

			milliSleep(detach_delay * 1000);

			probe.matchDfuOnly = true;
			dfuDevices = probe.find();

			if (!dfuDevices.size()) {
				ctx->pImpl->logAndThrow("Lost device after RESET?");
			} else if (dfuDevices.size()>1) {
				ctx->pImpl->logAndThrow(LogMsgType::MatchError_TooManyMatches, "More than one matching DFU capable USB device found! Try disconnecting all but one device");
			}

			dif = dfuDevices[0];

			/* Check for DFU mode device */
			if (!(dif->flags | DFU_IFF_DFU))
				ctx->pImpl->logAndThrow("Device is not in DFU mode");

			ctx->pImpl->log(LogLevel::Info, "Opening DFU USB Device...");
			dif->openDevice();
		} else {
			/* we're already in DFU mode, so we can skip the detach/reset
			 * procedure */
			/* If a match vendor/product was specified, use that as the runtime
			 * vendor/product, otherwise use the DFU mode vendor/product */
			runtime_usbId = probe.match_usbId;
			runtime_usbId.defaultsFrom(dif->usbId);
		}

	dfustate:
	#if 0
		ctx->pImpl->logf(LogLevel::Info, "Setting Configuration %u...", dif->configuration);
		if (libusb_set_configuration(dif->dev_handle, dif->configuration) < 0) {
			ctx->pImpl->logAndThrow("Cannot set configuration");
		}
	#endif
		ctx->pImpl->log(LogLevel::Info, "Claiming USB DFU Interface...");
		dif->claimInterface();

		ctx->pImpl->logf(LogLevel::Info, "Setting Alternate Setting #%d ...", dif->altsetting);
		if (libusb_set_interface_alt_setting(dif->dev_handle, dif->interface, dif->altsetting) < 0) {
			ctx->pImpl->logAndThrow("Cannot set alternate interface");
		}

	status_again:
		ctx->pImpl->log(LogLevel::Info, "Determining device status: ");
		if (dif->getStatus(&status ) < 0) {
			ctx->pImpl->logAndThrow("error get_status");
		}
		ctx->pImpl->logf(LogLevel::Info, "state = %s, status = %d",
			   dfu_state_to_string(status.bState), status.bStatus);

		milliSleep(status.bwPollTimeout);

		switch (status.bState) {
		case DFU_STATE_appIDLE:
		case DFU_STATE_appDETACH:
			ctx->pImpl->logAndThrow("Device still in Runtime Mode!");
			break;
		case DFU_STATE_dfuERROR:
			ctx->pImpl->log(LogLevel::Info, "dfuERROR, clearing status\n");
			if (dif->clearStatus() < 0) {
				ctx->pImpl->logAndThrow("error clear_status");
			}
			goto status_again;
			break;
		case DFU_STATE_dfuDNLOAD_IDLE:
		case DFU_STATE_dfuUPLOAD_IDLE:
			ctx->pImpl->log(LogLevel::Info, "aborting previous incomplete transfer\n");
			if (dif->abort() < 0) {
				ctx->pImpl->logAndThrow("can't send DFU_ABORT");
			}
			goto status_again;
			break;
		case DFU_STATE_dfuIDLE:
			ctx->pImpl->log(LogLevel::Info, "dfuIDLE, continuing\n");
			break;
		default:
			break;
		}

		if (DFU_STATUS_OK != status.bStatus ) {
			ctx->pImpl->logf(LogLevel::Warn, "DFU Status: '%s'\n",
				dfu_status_to_string(status.bStatus));
			/* Clear our status & try again. */
			if (dif->clearStatus() < 0)
				ctx->pImpl->logAndThrow("USB communication error");
			if (dif->getStatus(&status) < 0)
				ctx->pImpl->logAndThrow("USB communication error");
			if (DFU_STATUS_OK != status.bStatus)
				ctx->pImpl->logfAndThrow("Status is not OK: %d", status.bStatus);

			milliSleep(status.bwPollTimeout);
		}

		ctx->pImpl->logf(LogLevel::Info, "DFU mode device DFU version %04x\n",
			   dif->func_dfu.bcdDFUVersion);

		if (dif->func_dfu.bcdDFUVersion == 0x11a)
			dfuse_device = 1;


		if (!runtime_usbId.matchesSearch(file->getSearchId()) && !dif->usbId.matchesSearch(file->getSearchId()))
		{
			ctx->pImpl->logfAndThrow("Error: File ID %04x:%04x does "
				"not match device (%04x:%04x or %04x:%04x)",
				file->usbId.vendor, file->usbId.product,
				runtime_usbId.vendor, runtime_usbId.product,
				dif->usbId.vendor, dif->usbId.product);
		}
		if (dfuse_device || forceDfuse || file->bcdDFU == 0x11a) {
			DfuseController_download c(dif);
			c.file = file;
			c.opts = dfuseOpts;
			if (c.run()<0)
			{
				ctx->pImpl->logAndThrow("Download failed");
			}
		} else {
			DfuController_download c(dif);
			c.file = file;
			c.run();
			if (c.run()<0)
			{
				ctx->pImpl->logAndThrow("Download failed");
			}
		}


		if (finalReset)
		{
			if (dif->detach(1000) < 0) {
				/* Even if detach failed, just carry on to leave the
							   device in a known state */
				ctx->pImpl->log(LogLevel::Warn, "can't detach");
			}
			ctx->pImpl->log(LogLevel::Info, "Resetting USB to switch back to runtime mode");
			ret = libusb_reset_device(dif->dev_handle);
			if (ret < 0 && ret != LIBUSB_ERROR_NOT_FOUND) {
				ctx->pImpl->logAndThrow("error resetting after download");
			}
		}

		dif->closeDevice();
	}
	catch (...)
	{
		// TODO: check that all PackedData exceptions will be caught and converted to ctx->log calls with suitably descriptive error/warning messages

		ctx->pImpl->progress(1, "Failed");
		return false;
	}
	ctx->pImpl->progress(1, "Success");
	return true;
}

DfuDownloader::DfuDownloader(std::shared_ptr<Context> ctx, std::shared_ptr<DfuFile> file) :
	ctx(ctx), file(file), probe(ctx)
{}

}
