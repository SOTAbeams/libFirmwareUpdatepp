#ifndef ContextImpl_h
#define ContextImpl_h

#include "libFirmwareUpdate++/Context.hpp"

#include <mutex>
#include <atomic>
#include <stdexcept>
#include <cstdarg>

class libusb_context;

namespace FwUpd
{

class Error : public std::runtime_error
{
public:
	LogMsgType msgType = LogMsgType::Misc;
	using std::runtime_error::runtime_error;
	Error(LogMsgType t, std::string txt);
	virtual ~Error();
};

class ContextImpl
{
protected:
	Context::LogHandler logHandler;
	Context::ProgressHandler progressHandler;
	std::atomic<LogLevel> minLogLevel;
	libusb_context *libusb_ctx = nullptr;
	std::string productName;

	std::recursive_mutex mtx;

	void updateLibUsbLogLevel();

public:


	void setProgressHandler(Context::ProgressHandler f);
	void progress(float newProgress, std::string desc="");
	void progress(uint64_t current, uint64_t total, std::string desc="");

	void setLogHandler(Context::LogHandler f);
	void setMinLogLevel(LogLevel x);
	LogLevel getMinLogLevel() const;

	void setProductName(std::string name);
	std::string getProductName();

	bool shouldLog(LogLevel x) const;
	void defaultLogHandler(const LogMsg &msg);
	void log(LogLevel level, LogMsgType t, std::string txt);
	void log(LogLevel level, std::string txt);
	void logf(LogLevel level, LogMsgType t, const char *fmt, va_list args);
	void logf(LogLevel level, LogMsgType t, const char *fmt, ...);
	void logf(LogLevel level, const char *fmt, ...);
	// For LogLevel::Error, functions which log the message and throw an exception
	[[noreturn]] void logAndThrow(LogMsgType t, std::string txt);
	[[noreturn]] void logAndThrow(std::string txt);
	[[noreturn]] void logfAndThrow(LogMsgType t, const char *fmt, ...);
	[[noreturn]] void logfAndThrow(const char *fmt, ...);

	libusb_context *getLibUsbCtx();
	void assert_usbXferOk(int ret, std::string txt="libusb_control_transfer failed");
	void assert_usbXferLength(int requiredLength, int ret, std::string txt);

	ContextImpl();
	virtual ~ContextImpl();
};

}

#endif
