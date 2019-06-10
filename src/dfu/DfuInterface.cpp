/*
 * Low-level DFU communication routines, originally taken from
 * $Id: dfu.c,v 1.3 2006/06/20 06:28:04 schmidtw Exp $
 * (part of dfu-programmer).
 *
 * Copyright 2005-2006 Weston Schmidt <weston_schmidt@alumni.purdue.edu>
 * Copyright 2011-2014 Tormod Volden <debian.tormod@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>

#include <libusb.h>

#include "libFirmwareUpdate++/dfu/DfuInterface.hpp"
#include "quirks.hpp"
#include "PackedData.hpp"
#include "ContextImpl.hpp"
#include "usb_dfu.hpp"

static int dfu_timeout = 5000;  /* 5 seconds - default */


const char* dfu_state_to_string( int state )
{
	const char *message;

	switch (state) {
	case STATE_APP_IDLE:
		message = "appIDLE";
		break;
	case STATE_APP_DETACH:
		message = "appDETACH";
		break;
	case STATE_DFU_IDLE:
		message = "dfuIDLE";
		break;
	case STATE_DFU_DOWNLOAD_SYNC:
		message = "dfuDNLOAD-SYNC";
		break;
	case STATE_DFU_DOWNLOAD_BUSY:
		message = "dfuDNBUSY";
		break;
	case STATE_DFU_DOWNLOAD_IDLE:
		message = "dfuDNLOAD-IDLE";
		break;
	case STATE_DFU_MANIFEST_SYNC:
		message = "dfuMANIFEST-SYNC";
		break;
	case STATE_DFU_MANIFEST:
		message = "dfuMANIFEST";
		break;
	case STATE_DFU_MANIFEST_WAIT_RESET:
		message = "dfuMANIFEST-WAIT-RESET";
		break;
	case STATE_DFU_UPLOAD_IDLE:
		message = "dfuUPLOAD-IDLE";
		break;
	case STATE_DFU_ERROR:
		message = "dfuERROR";
		break;
	default:
		message = NULL;
		break;
	}

	return message;
}

/* Chapter 6.1.2 */
static const char *dfu_status_names[] = {
	/* DFU_STATUS_OK */
	"No error condition is present",
	/* DFU_STATUS_errTARGET */
	"File is not targeted for use by this device",
	/* DFU_STATUS_errFILE */
	"File is for this device but fails some vendor-specific test",
	/* DFU_STATUS_errWRITE */
	"Device is unable to write memory",
	/* DFU_STATUS_errERASE */
	"Memory erase function failed",
	/* DFU_STATUS_errCHECK_ERASED */
	"Memory erase check failed",
	/* DFU_STATUS_errPROG */
	"Program memory function failed",
	/* DFU_STATUS_errVERIFY */
	"Programmed memory failed verification",
	/* DFU_STATUS_errADDRESS */
	"Cannot program memory due to received address that is out of range",
	/* DFU_STATUS_errNOTDONE */
	"Received DFU_DNLOAD with wLength = 0, but device does not think that it has all data yet",
	/* DFU_STATUS_errFIRMWARE */
	"Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations",
	/* DFU_STATUS_errVENDOR */
	"iString indicates a vendor specific error",
	/* DFU_STATUS_errUSBR */
	"Device detected unexpected USB reset signalling",
	/* DFU_STATUS_errPOR */
	"Device detected unexpected power on reset",
	/* DFU_STATUS_errUNKNOWN */
	"Something went wrong, but the device does not know what it was",
	/* DFU_STATUS_errSTALLEDPKT */
	"Device stalled an unexpected request"
};


const char *dfu_status_to_string(int status)
{
	if (status > DFU_STATUS_errSTALLEDPKT)
		return "INVALID";
	return dfu_status_names[status];
}


namespace FwUpd
{

int DfuInterface::dfuXferIn(uint8_t bRequest, uint16_t wValue, unsigned char *data, uint16_t wLength)
{
	return libusb_control_transfer(dev_handle,
								   /* bmRequestType */ LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
								   /* bRequest      */ bRequest,
								   /* wValue        */ wValue,
								   /* wIndex        */ interface,
								   /* Data          */ data,
								   /* wLength       */ wLength,
								   dfu_timeout );
}

int DfuInterface::dfuXferOut(uint8_t bRequest, uint16_t wValue, unsigned char *data, uint16_t wLength)
{
	return libusb_control_transfer(dev_handle,
								   /* bmRequestType */ LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
								   /* bRequest      */ bRequest,
								   /* wValue        */ wValue,
								   /* wIndex        */ interface,
								   /* Data          */ data,
								   /* wLength       */ wLength,
								   dfu_timeout );
}

int DfuInterface::download(uint16_t blockNum, unsigned char *data, uint16_t length)
{
	return dfuXferOut(DFU_DNLOAD, blockNum, data, length);
}

int DfuInterface::upload(uint16_t blockNum, unsigned char *data, uint16_t length)
{
	return dfuXferIn(DFU_UPLOAD, blockNum, data, length);
}

int DfuInterface::detach(uint16_t timeout)
{
	return dfuXferOut(DFU_DETACH, timeout, nullptr, 0);
}

int DfuInterface::getStatus(dfu_status *status)
{
	unsigned char buffer[6];
	int result;

	/* Initialize the status data structure */
	status->bStatus       = DFU_STATUS_ERROR_UNKNOWN;
	status->bwPollTimeout = 0;
	status->bState        = STATE_DFU_ERROR;
	status->iString       = 0;

	result = dfuXferIn(DFU_GETSTATUS, 0, buffer, 6);

	if( 6 == result ) {
		PackedData::Reader d(buffer, 6);
		status->bStatus = d.read_u8();
		if (quirks & QUIRK_POLLTIMEOUT)
		{
			status->bwPollTimeout = DEFAULT_POLLTIMEOUT;
			d.skip(3);
		}
		else
		{
			status->bwPollTimeout = d.read_u24l();
		}
		status->bState  = d.read_u8();
		status->iString = d.read_u8();
	}

	return result;
}

int DfuInterface::clearStatus()
{
	return dfuXferOut(DFU_CLRSTATUS, 0, nullptr, 0);
}

int DfuInterface::getState()
{
	int result;
	unsigned char buffer[1];

	result = dfuXferIn(DFU_GETSTATE, 0, buffer, 1);

	/* Return the error if there is one. */
	if (result < 1)
		return -1;

	/* Return the state. */
	return buffer[0];
}

int DfuInterface::abort()
{
	return dfuXferOut(DFU_ABORT, 0, nullptr, 0);
}

void DfuInterface::openDevice()
{
	if (isOpen)
		return;
	int ret = libusb_open(dev, &dev_handle);
	if (ret || !dev_handle)
		ctx->pImpl->logAndThrow(LogMsgType::UsbIoError, "Cannot open device");
	isOpen = true;
}

void DfuInterface::closeDevice()
{
	if (!isOpen)
		return;
	releaseInterface();
	libusb_close(dev_handle);
	dev_handle = nullptr;
	isOpen = false;
}

void DfuInterface::claimInterface()
{
	if (isClaimed)
		return;
	if (libusb_claim_interface(dev_handle, interface) < 0)
		ctx->pImpl->logAndThrow(LogMsgType::UsbIoError, "Cannot claim interface");
	isClaimed = true;
}

void DfuInterface::releaseInterface()
{
	if (!isClaimed)
		return;
	libusb_release_interface(dev_handle, interface);
	isClaimed = false;
}

DfuInterface::~DfuInterface()
{
	if (isClaimed)
		releaseInterface();
	if (isOpen)
		closeDevice();
	libusb_unref_device(dev);
}


/*
 * void print_dfu_if(DfuInterface* dfu_if)
{
	std::string devPath = DfuFinderImpl::get_path(dfu_if->dev);
	printf("Found %s: [%04x:%04x] ver=%04x, devnum=%u, cfg=%u, intf=%u, "
	       "path=\"%s\", alt=%u, name=\"%s\", serial=\"%s\"\n",
	       dfu_if->flags & DFU_IFF_DFU ? "DFU" : "Runtime",
	       dfu_if->usbId.vendor, dfu_if->usbId.product,
	       dfu_if->bcdDevice, dfu_if->devnum,
	       dfu_if->configuration, dfu_if->interface,
	       devPath.c_str(),
	       dfu_if->altsetting, dfu_if->alt_name.c_str(),
	       dfu_if->serial_name.c_str());
}

*/


}
