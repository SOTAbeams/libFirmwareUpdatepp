#ifndef libFirmwareUpdate_Context_h
#define libFirmwareUpdate_Context_h

#include "libFirmwareUpdate++/LogMsg.hpp"

#include <functional>
#include <string>
#include <memory>

namespace FwUpd
{

class ContextImpl;

class Context
{
public:
	ContextImpl *pImpl;

public:
	using ProgressHandler = std::function<void(float x, std::string desc)>;
	void setProgressHandler(ProgressHandler f);

	using LogHandler = std::function<void(const LogMsg &msg)>;
	void setLogHandler(LogHandler f);
	void setMinLogLevel(LogLevel x);
	LogLevel getMinLogLevel() const;

	// For customisation of log/progress messages, to be used instead of generic names like "DFU device". Not used much yet.
	void setProductName(std::string name);
	std::string getProductName() const;

	Context();
	~Context();

};


}

#endif
