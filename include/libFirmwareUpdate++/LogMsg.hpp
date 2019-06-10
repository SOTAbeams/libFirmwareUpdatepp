#ifndef libFirmwareUpdate_LogMsg_h
#define libFirmwareUpdate_LogMsg_h

#include <string>

namespace FwUpd
{

enum class LogLevel
{
	Verbose3,
	Verbose2,
	Verbose,
	Info,
	Warn,
	Error,
};


enum class LogMsgType
{
	Misc,

	UsbIoError,
	FileIoError,
	FileFormatError,
	InvalidOptions,// Invalid settings supplied
	MatchError_NoMatches,
	MatchError_TooManyMatches,


};

class LogMsg
{
public:
	LogLevel level;
	LogMsgType type;
	std::string txt;
};


}

#endif
