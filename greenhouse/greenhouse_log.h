/**
   only print stuff to serial if DEBUG-build,
   disabling Serial frees alot of memory!
*/
#ifndef __GREENHOUSE_LOG__
#define __GREENHOUSE_LOG__ 

#define DEBUG 1

#if (DEBUG > 0)
#define SERIAL_BEGIN(_sbaud)\
  Serial.begin(_sbaud);
#else
#define SERIAL_BEGIN(_sbaud)
#endif

#if (DEBUG > 0)
#define LOG_ERRORLN(_log)\
  Serial.println(_log);
#define LOG_ERROR(_log)\
  Serial.print(_log);
#else
#define LOG_ERRORLN(_log)
#define LOG_ERROR(_log)
#endif

#if (DEBUG > 1)
#define LOG_DEBUGLN(_log)\
  Serial.println(_log);
#define LOG_DEBUG(_log)\
  Serial.print(_log);
#else
#define LOG_DEBUGLN(_log)
#define LOG_DEBUG(_log)
#endif

#if (DEBUG > 2)
#define LOG_DEBUGLN2(_log)\
  Serial.println(_log);
#define LOG_DEBUG2(_log)\
  Serial.print(_log);
#else
#define LOG_DEBUGLN2(_log)
#define LOG_DEBUG2(_log)
#endif

#endif //__GREENHOUSE_LOG__
