#include "ContextImpl.hpp"

#include <libusb.h>
#include <iostream>
#include <vector>

namespace FwUpd
{

static std::string printfString(const char *fmt, va_list args)
{
	std::vector<char> tmp;
	tmp.reserve(512);
	while (1)
	{
		int charsWritten = vsnprintf(tmp.data(), tmp.capacity(), fmt, args);
		// negative return value indicates an encoding error, not much can be done here to fix that
		if (charsWritten < 0)
			break;
		// Sanity limit, since strings used with this are not normally enormously long
		if (charsWritten > (1<<20))
			break;
		// Check if the entire string was written
		if (charsWritten < static_cast<int>(tmp.capacity()))
			break;
		// Buffer was not large enough, so increase size and try again
		tmp.reserve(tmp.capacity()*2);
	}
	return std::string(tmp.data());
}



Error::Error(LogMsgType t, std::string txt) : Error(txt)
{
	msgType = t;
}

Error::~Error()
{

}

void ContextImpl::setLogHandler(Context::LogHandler f)
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	logHandler = f;
}

void ContextImpl::updateLibUsbLogLevel()
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	if (!libusb_ctx)
		return;
	if (shouldLog(LogLevel::Verbose3))
		libusb_set_debug(libusb_ctx, 255);
	else
		libusb_set_debug(libusb_ctx, LIBUSB_LOG_LEVEL_NONE);
}

void ContextImpl::setProgressHandler(Context::ProgressHandler f)
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	progressHandler = f;
}

void ContextImpl::progress(float newProgress, std::string desc)
{
	if (progressHandler)
		progressHandler(newProgress, desc);
}

void ContextImpl::progress(uint64_t current, uint64_t total, std::string desc)
{
	progress(static_cast<float>(current)/total, desc);
}

void ContextImpl::setMinLogLevel(LogLevel x)
{
	minLogLevel = x;
	updateLibUsbLogLevel();
}

LogLevel ContextImpl::getMinLogLevel() const
{
	return minLogLevel;
}

void ContextImpl::setProductName(std::string name)
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	productName = name;
}

std::string ContextImpl::getProductName()
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	return productName;

}

bool ContextImpl::shouldLog(LogLevel x) const
{
	return (x >= minLogLevel);
}

void ContextImpl::defaultLogHandler(const LogMsg &msg)
{
	const char *prefix = nullptr;
	switch (msg.level)
	{
	default:
	case LogLevel::Verbose:
		break;
	case LogLevel::Info:
		prefix = "Info";
		break;
	case LogLevel::Warn:
		prefix = "Warning";
		break;
	case LogLevel::Error:
		prefix = "Error";
		break;
	}
	std::ostream &dst = (msg.level >= LogLevel::Warn ? std::cerr : std::cout);
	if (prefix)
		dst << prefix << ": ";
	dst << msg.txt << std::endl;
}

void ContextImpl::log(LogLevel level, LogMsgType t, std::string txt)
{
	if (!shouldLog(level))
		return;

	LogMsg msg;
	msg.level = level;
	msg.type = t;
	msg.txt = txt;

	{
		std::lock_guard<std::recursive_mutex> lk(mtx);
		if (logHandler)
			logHandler(msg);
		else
			defaultLogHandler(msg);
	}
}

void ContextImpl::log(LogLevel level, std::string txt)
{
	log(level, LogMsgType::Misc, txt);
}

void ContextImpl::logf(LogLevel level, LogMsgType t, const char *fmt, va_list args)
{
	if (!shouldLog(level))
		return;
	log(level, t, printfString(fmt, args));
}

void ContextImpl::logf(LogLevel level, LogMsgType t, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	logf(level, t, fmt, args);
	va_end(args);
}

void ContextImpl::logf(LogLevel level, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	logf(level, LogMsgType::Misc, fmt, args);
	va_end(args);
}

void ContextImpl::logAndThrow(LogMsgType t, std::string txt)
{
	log(LogLevel::Error, t, txt);
	throw Error(txt);
}

void ContextImpl::logAndThrow(std::string txt)
{
	logAndThrow(LogMsgType::Misc, txt);
}

void ContextImpl::logfAndThrow(LogMsgType t, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	logAndThrow(t, printfString(fmt, args));
	va_end(args);
}

void ContextImpl::logfAndThrow(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	logAndThrow(LogMsgType::Misc, printfString(fmt, args));
	va_end(args);
}

libusb_context *ContextImpl::getLibUsbCtx()
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	if (!libusb_ctx)
	{
		int ret = libusb_init(&libusb_ctx);
		if (ret)
		{
			logf(LogLevel::Error, "libusb_init return code: %d", ret);
			logAndThrow("unable to initialize libusb");
		}
		updateLibUsbLogLevel();
	}
	return libusb_ctx;
}

void ContextImpl::assert_usbXferOk(int ret, std::string txt)
{
	if (ret < 0)
		logAndThrow(LogMsgType::UsbIoError, txt);
}

void ContextImpl::assert_usbXferLength(int requiredLength, int ret, std::string txt)
{
	if (ret < 0)
		logAndThrow(LogMsgType::UsbIoError, txt + " - libusb_control_transfer failed");
	else if (ret != requiredLength)
		logAndThrow(LogMsgType::UsbIoError, txt);
}

ContextImpl::ContextImpl()
{
	minLogLevel = LogLevel::Warn;
	productName = "USB device";
}

ContextImpl::~ContextImpl()
{
	std::lock_guard<std::recursive_mutex> lk(mtx);
	if (libusb_ctx)
	{
		libusb_exit(libusb_ctx);
		libusb_ctx = nullptr;
	}
}




}
