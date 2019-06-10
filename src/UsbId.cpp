#include "libFirmwareUpdate++/UsbId.hpp"

namespace FwUpd
{

bool UsbId::matchesSearch(const UsbId &searchId)
{
	if (searchId.vendor>=0 && searchId.vendor != vendor)
		return false;
	if (searchId.product>=0 && searchId.product != product)
		return false;
	return true;
}

bool UsbId::matchesSearch_wildSelfFFFF(const UsbId &searchId)
{
	if (searchId.vendor>=0 && vendor!=0xFFFF && searchId.vendor != vendor)
		return false;
	if (searchId.product>=0 && product!=0xFFFF && searchId.product != product)
		return false;
	return true;
}

void UsbId::clear()
{
	vendor = product = -1;
}

void UsbId::mergeFrom(const UsbId &src)
{
	if (src.vendor>=0)
		vendor = src.vendor;
	if (src.product>=0)
		product = src.product;
}

void UsbId::defaultsFrom(const UsbId &src)
{
	// Don't need to check whether src is initialised, since it is only copied if this id uninitialised, and overwriting -1 with -1 means no change.
	if (!hasVendor())
		vendor = src.vendor;
	if (!hasProduct())
		product = src.product;
}

UsbId::UsbId()
{
	clear();
}

UsbId::UsbId(int vendor, int product) : vendor(vendor), product(product)
{}

}
