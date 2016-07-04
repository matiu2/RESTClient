#pragma once


#include <string>
#include <iostream>
 
namespace RESTClient {

// All log levels
#define TRACE 1
#define DEBUG 2
#define INFO 3
#define WARN 4
#define ERROR 5
#define FATAL 6
#define NONE 7

#ifndef MIN_LOG_LEVEL
#define MIN_LOG_LEVEL NONE
#endif

#ifdef LOG_LOCATION
#define LOG(LEVEL, ARG)                                                        \
  std::clog << LEVEL << " - Line: " << __LINE__ << " - File: " << __FILE__     \
            << " - Function: " << __FUNCTION__ << " - Msg: " << ARG            \
            << std::endl;
#else
#define LOG(LEVEL, ARG) std::clog << LEVEL << " - " << ARG << std::endl;
#endif

#if MIN_LOG_LEVEL <= TRACE
#define LOG_TRACE(ARG) LOG("TRACE", ARG);
#else
#define LOG_TRACE(ARG)
#endif

#if MIN_LOG_LEVEL <= DEBUG
#define LOG_DEBUG(ARG) LOG("DEBUG", ARG);
#else
#define LOG_DEBUG(ARG)
#endif

#if MIN_LOG_LEVEL <= INFO
#define LOG_INFO(ARG) LOG("INFO", ARG);
#else
#define LOG_INFO(ARG)
#endif

#if MIN_LOG_LEVEL <= WARN
#define LOG_WARN(ARG) LOG("WARNING", ARG);
#else
#define LOG_WARN(ARG)
#endif

#if MIN_LOG_LEVEL <= ERROR
#define LOG_ERROR(ARG)                                                         \
  LOG("ERROR", ARG)                                                            \
  std::stringstream __msg;                                                     \
  __msg << "ERROR: " << ARG;                                                   \
  throw std::runtime_error(__msg.str());
#define LOG_ERROR2(ARG)                                                        \
  LOG("ERROR", ARG)                                                            \
  __msg << "ERROR: " << ARG;                                                   \
  throw std::runtime_error(__msg.str());
#else
#define LOG_ERROR(ARG)                                                         \
  std::stringstream __msg;                                                     \
  __msg << "ERROR: " << ARG;                                                   \
  throw std::runtime_error(__msg.str());
#define LOG_ERROR2(ARG)                                                        \
  LOG("ERROR", ARG)                                                            \
  __msg << "ERROR: " << ARG;                                                   \
  throw std::runtime_error(__msg.str());
#endif

#if MIN_LOG_LEVEL <= FATAL
#define LOG_FATAL(ARG)                                                         \
  LOG("FATAL", ARG);                                                           \
  std::stringstream __msg;                                                     \
  __msg << "FATAL ERROR: " << ARG;                                             \
  throw std::runtime_error(__msg.str());
#define LOG_FATAL2(ARG)                                                        \
  LOG("FATAL", ARG);                                                           \
  __msg << "FATAL ERROR: " << ARG;                                             \
  throw std::runtime_error(__msg.str());
#else
#define LOG_FATAL(ARG)                                                         \
  std::stringstream __msg;                                                     \
  __msg << "FATAL ERROR: " << ARG;                                             \
  throw std::runtime_error(__msg.str());
#define LOG_FATAL2(ARG)                                                        \
  __msg << "FATAL ERROR: " << ARG;                                             \
  throw std::runtime_error(__msg.str());
#endif

} /* RESTClient */
