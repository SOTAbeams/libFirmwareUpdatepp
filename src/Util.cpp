#include "Util.hpp"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <chrono>
#include <thread>

namespace FwUpd
{

void printfStream(std::ostream &stream, const char *fmt, ...)
{
	char tmp[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, args);
	tmp[sizeof(tmp)-1] = 0;
	va_end(args);
	stream << std::string(tmp);
}

void milliSleep(uint32_t msecs)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(msecs));
}

}
