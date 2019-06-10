#include "libFirmwareUpdate++/Context.hpp"
#include "ContextImpl.hpp"

#include <libusb.h>
#include <iostream>

namespace FwUpd
{

void Context::setProgressHandler(Context::ProgressHandler f)
{
	pImpl->setProgressHandler(f);
}

void Context::setLogHandler(Context::LogHandler f)
{
	pImpl->setLogHandler(f);
}

void Context::setMinLogLevel(LogLevel x)
{
	pImpl->setMinLogLevel(x);
}

LogLevel Context::getMinLogLevel() const
{
	return pImpl->getMinLogLevel();
}

void Context::setProductName(std::string name)
{
	pImpl->setProductName(name);
}

std::string Context::getProductName() const
{
	return pImpl->getProductName();
}

Context::Context()
{
	pImpl = new ContextImpl();
}

Context::~Context()
{
	delete pImpl;
}

}
