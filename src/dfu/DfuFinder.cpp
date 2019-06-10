#include "libFirmwareUpdate++/dfu.hpp"
#include "DfuFinderImpl.hpp"
#include "ContextImpl.hpp"

namespace FwUpd
{

DfuFinder::Results DfuFinder::find()
{
	Results dst;
	DfuFinderImpl impl;
	impl.ctxi = ctx->pImpl;
	impl.usbctx = ctx->pImpl->getLibUsbCtx();
	impl.f = this;
	impl.results = &dst;
	impl.find();
	return dst;
}

DfuFinder::DfuFinder(std::shared_ptr<Context> ctx) : ctx(ctx)
{
	reset();
}

DfuFinder::~DfuFinder()
{

}

void DfuFinder::reset()
{
	match_path = "";
	match_usbId.clear();
	match_usbId_dfu.clear();
	match_config_index = -1;
	match_iface_index = -1;
	match_iface_alt_index = -1;
	match_iface_alt_name = "";
	match_serial = "";
	match_serial_dfu = "";
	matchDfuOnly = false;
}




}

