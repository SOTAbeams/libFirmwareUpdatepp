/*
 *  Simple quirk system for dfu-util
 *
 *  Copyright 2010-2014 Tormod Volden
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

#include <stdint.h>
#include "quirks.hpp"

#define VENDOR_OPENMOKO 0x1d50 /* Openmoko Freerunner / GTA02 */
#define VENDOR_FIC      0x1457 /* Openmoko Freerunner / GTA02 */
#define VENDOR_VOTI	0x16c0 /* OpenPCD Reader */
#define VENDOR_LEAFLABS 0x1eaf /* Maple */
#define VENDOR_SIEMENS 0x0908 /* Siemens AG */
#define VENDOR_MIDIMAN  0x0763 /* Midiman */

#define PRODUCT_FREERUNNER_FIRST 0x5117
#define PRODUCT_FREERUNNER_LAST  0x5126
#define PRODUCT_OPENPCD	0x076b
#define PRODUCT_MAPLE3	0x0003 /* rev 3 and 5 */
#define PRODUCT_PXM40	0x02c4 /* Siemens AG, PXM 40 */
#define PRODUCT_PXM50	0x02c5 /* Siemens AG, PXM 50 */
#define PRODUCT_TRANSIT	0x2806 /* M-Audio Transit (Midiman) */

uint16_t get_quirks(uint16_t vendor, uint16_t product, uint16_t bcdDevice)
{
	uint16_t quirks = 0;

	/* Device returns bogus bwPollTimeout values */
	if ((vendor == VENDOR_OPENMOKO || vendor == VENDOR_FIC) &&
	    product >= PRODUCT_FREERUNNER_FIRST &&
	    product <= PRODUCT_FREERUNNER_LAST)
		quirks |= QUIRK_POLLTIMEOUT;

	if (vendor == VENDOR_VOTI &&
	    product == PRODUCT_OPENPCD)
		quirks |= QUIRK_POLLTIMEOUT;

	/* Reports wrong DFU version in DFU descriptor */
	if (vendor == VENDOR_LEAFLABS &&
	    product == PRODUCT_MAPLE3 &&
	    bcdDevice == 0x0200)
		quirks |= QUIRK_FORCE_DFU11;

	/* old devices(bcdDevice == 0) return bogus bwPollTimeout values */
	if (vendor == VENDOR_SIEMENS &&
	    (product == PRODUCT_PXM40 || product == PRODUCT_PXM50) &&
	    bcdDevice == 0)
		quirks |= QUIRK_POLLTIMEOUT;

	/* M-Audio Transit returns bogus bwPollTimeout values */
	if (vendor == VENDOR_MIDIMAN &&
	    product == PRODUCT_TRANSIT)
		quirks |= QUIRK_POLLTIMEOUT;

	return (quirks);
}
