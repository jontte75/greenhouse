#ifndef __LOGGING_H__
#define __LOGGING_H__

#include "Arduino.h"
#include <HardwareSerial.h>

enum logLevel
{
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG,
    LOG_DEBUG2
};

class Log
{
  public:
    Log(HardwareSerial& _serial, short baud, logLevel ll);
    void print(const char *fmt, ...);
    void print(const float& value);
    logLevel getLogLevel();
    void setLogLevel(logLevel ll);

  private:
    HardwareSerial& serial;
    logLevel level;
};

template<typename T> inline Log &operator <<(Log &obj, T arg) { obj.print(arg); return obj; } 

#define ERROR(_log, ...)\
  if (_log.getLogLevel() >= LOG_ERROR){\
    _log << "[E] " << __VA_ARGS__;\
  }

#define WARNING(_log, ...)\
  if (_log.getLogLevel() >= LOG_WARNING){\
    _log << "[I] " << __VA_ARGS__;\
  }

#define INFO(_log, ...)\
  if (_log.getLogLevel() >= LOG_INFO){\
    _log << "[I] " << __VA_ARGS__;\
  }

#define DEBUG(_log, ...)\
  if (_log.getLogLevel() >= LOG_DEBUG){\
    _log << "[D] " << __VA_ARGS__;\
  }

#define DEBUG2(_log, ...)\
  if (_log.getLogLevel() >= LOG_DEBUG2){\
    _log << "[D2] " << __VA_ARGS__;\
  }

#endif