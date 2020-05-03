#include "./log.h"

Log::Log(HardwareSerial& _serial, short baud, logLevel ll) : serial(_serial), level(ll)
{
    serial.begin(baud);
}

void Log::print(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    char buf[vsnprintf(NULL, 0, fmt, va) + 1];
    vsprintf(buf, fmt, va);
    serial.print(buf);
    va_end(va);
}

void Log::print(const float& value)
{
    serial.print(value);
}

logLevel Log::getLogLevel()
{
    return level;
}

void Log::setLogLevel(logLevel lvl)
{
    level = lvl;
}
