#ifndef libFirmwareUpdate_UsbId_h
#define libFirmwareUpdate_UsbId_h

namespace FwUpd
{

class UsbId
{
public:
	int vendor;
	int product;

	bool hasProduct() const
	{
		return product >= 0;
	}
	bool hasVendor() const
	{
		return vendor >= 0;
	}

	// Checks whether both parts of this id match searchId. searchId parts<0 match anything.
	bool matchesSearch(const UsbId &searchId);
	// Checks whether both parts of this id match searchId.  searchId parts<0 match anything. parts==0xFFFF in this id match anything.
	bool matchesSearch_wildSelfFFFF(const UsbId &searchId);
	void clear();

	// Overwrites this id with any initialised (x>=0) parts of src. If a part of src is not initialised, no changes are made to that part of this id.
	void mergeFrom(const UsbId &src);
	// Sets any uninitialised parts of this id to the corresponding parts of src.
	void defaultsFrom(const UsbId &src);

	UsbId();
	UsbId(int vendor, int product);
};

}

#endif
