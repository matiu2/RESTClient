#pragma once

#include <string>
#include <iostream>

namespace RESTClient {

// All log levels
#define LOG_LEVEL_TRACE 1
#define LOG_LEVEL_DEBUG 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_WARN 4
#define LOG_LEVEL_ERROR 5
#define LOG_LEVEL_FATAL 6
#define LOG_LEVEL_NONE 7

#ifndef MIN_LOG_LEVEL
#define MIN_LOG_LEVEL LOG_LEVEL_NONE
#endif

#ifdef LOG_LOCATION
#define LOG(LEVEL, ARG)                                                        \
  std::clog << LEVEL << " - Line: " << __LINE__ << " - File: " << __FILE__     \
            << " - Function: " << __FUNCTION__ << " - Msg: " << ARG            \
            << std::endl;
#else
#define LOG(LEVEL, ARG) std::clog << LEVEL << " - " << ARG << std::endl;
#endif

#if MIN_LOG_LEVEL <= LOG_LEVEL_TRACE
#define LOG_TRACE(ARG) LOG("TRACE", ARG);
#else
#define LOG_TRACE(ARG)
#endif

#if MIN_LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(ARG) LOG("DEBUG", ARG);
#else
#define LOG_DEBUG(ARG)
#endif

#if MIN_LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(ARG) LOG("INFO", ARG);
#else
#define LOG_INFO(ARG)
#endif

#if MIN_LOG_LEVEL <= LOG_LEVEL_WARN
#define LOG_WARN(ARG) LOG("WARNING", ARG);
#else
#define LOG_WARN(ARG)
#endif

#if MIN_LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(ARG)                                                         \
  {                                                                            \
    LOG("ERROR", ARG)                                                          \
    std::stringstream __msg;                                                   \
    __msg << "ERROR: " << ARG;                                                 \
    throw std::runtime_error(__msg.str());                                     \
  }
#else
#define LOG_ERROR(ARG)                                                         \
  {                                                                            \
    std::stringstream __msg;                                                   \
    __msg << "ERROR: " << ARG;                                                 \
    throw std::runtime_error(__msg.str());                                     \
  }
#endif

#if MIN_LOG_LEVEL <= LOG_LEVEL_FATAL
#define LOG_FATAL(ARG)                                                         \
  {                                                                            \
    LOG("FATAL", ARG);                                                         \
    std::stringstream __msg;                                                   \
    __msg << "FATAL ERROR: " << ARG;                                           \
    throw std::runtime_error(__msg.str());                                     \
  }
#else
#define LOG_FATAL(ARG)                                                         \
  {                                                                            \
    std::stringstream __msg;                                                   \
    __msg << "FATAL ERROR: " << ARG;                                           \
    throw std::runtime_error(__msg.str());                                     \
  }
#endif

} /* RESTClient */
