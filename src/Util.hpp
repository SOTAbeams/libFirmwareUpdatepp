#ifndef fwupd_Util_h
#define fwupd_Util_h

#include <cstdint>
#include <iosfwd>

namespace FwUpd
{

void printfStream(std::ostream &stream, const char *fmt, ...);
void milliSleep(uint32_t msecs);

}


#endif
