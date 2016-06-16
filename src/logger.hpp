#pragma once


#include <string>
#include <iostream>
 
namespace RESTClient {

// All log levels
#define LL_NONE 0
#define LL_TRACE 1
#define LL_DEBUG 2
#define LL_INFO 3
#define LL_WARN 4
#define LL_ERROR 5
#define ll_trace 6

// Work out our log level
#if MIN_LOG_LEVEL == TRACE
#define LOG_LEVEL LL_TRACE
#elif MIN_LOG_LEVEL == DEBUG
#define LOG_LEVEL LL_DEBUG
#elif MIN_LOG_LEVEL == INFO
#define LOG_LEVEL LL_INFO
#elif MIN_LOG_LEVEL == WARN
#define LOG_LEVEL LL_WARN
#elif MIN_LOG_LEVEL == ERROR
#define LOG_LEVEL LL_ERROR
#elif MIN_LOG_LEVEL == FATAL
#define LOG_LEVEL LL_FATAL
#else
#define LOG_LEVEL LL_NONE
#endif

#ifdef LOG_LOCATION
#define LOG(LEVEL, ARG)                                                        \
  std::clog << LEVEL << " - Line: " << __LINE__ << " - File: " << __FILE__     \
            << " - Function: " << __FUNCTION__ << " - Msg: " << ARG            \
            << std::endl;
#else
#define LOG(LEVEL, ARG) std::clog << LEVEL << " - " ARG << std::endl;
#endif

#if LOG_LEVEL >= LL_TRACE
#define LOG_TRACE(ARG) LOG("TRACE", ARG);
#else
#define LOG_TRACE(ARG)
#endif

#if LOG_LEVEL >= LL_DEBUG
#define LOG_DEBUG(ARG) LOG("DEBUG", ARG);
#else
#define LOG_DEBUG(ARG)
#endif

#if LOG_LEVEL >= LL_INFO
#define LOG_INFO(ARG)  LOG("INFO", ARG);
#else
#define LOG_INFO(ARG)
#endif

#if LOG_LEVEL >= LL_WARN
#define LOG_WARN(ARG)  LOG("WARNING", ARG);
#else
#define LOG_WARN(ARG)
#endif

#if LOG_LEVEL >= LL_ERROR
#define LOG_ERROR(ARG)                                                         \
  LOG("ERROR", ARG)                                                            \
  std::stringstream msg;                                                       \
  msg << "ERROR: " << ARG;                                                     \
  throw std::runtime_error(msg.str());
#else
#define LOG_ERROR(ARG)                                                         \
  std::stringstream msg;                                                       \
  msg << "ERROR: " << ARG;                                                     \
  throw std::runtime_error(msg.str());
#endif

#if LOG_LEVEL >= LL_FATAL
#define LOG_FATAL(ARG)                                                         \
  LOG("FATAL", ARG);                                                           \
  std::stringstream msg;                                                       \
  msg << "FATAL ERROR: " << ARG;                                               \
  throw std::runtime_error(msg.str());
#else
#define LOG_FATAL(ARG)                                                         \
  std::stringstream msg;                                                       \
  msg << "FATAL ERROR: " << ARG;                                               \
  throw std::runtime_error(msg.str());
#endif

#define LOG_DATA_CHANNEL(chan) BOOST_LOG_CHANNEL(sysLogger::get(), chan);

/// Data Log macros. Does not include LINE, FILE, FUNCTION.
/// TRACE < DEBUG < INFO < WARN < ERROR < FATAL
#define LOG_DATA_TRACE(ARG) BOOST_LOG_SEV(dataLogger::get(), boost::log::trivial::trace) << ARG
#define LOG_DATA_DEBUG(ARG) BOOST_LOG_SEV(dataLogger::get(), boost::log::trivial::debug) << ARG
#define LOG_DATA_INFO(ARG)  BOOST_LOG_SEV(dataLogger::get(), boost::log::trivial::info) << ARG
#define LOG_DATA_WARN(ARG)  BOOST_LOG_SEV(dataLogger::get(), boost::log::trivial::warning) << ARG
#define LOG_DATA_ERROR(ARG) BOOST_LOG_SEV(dataLogger::get(), boost::log::trivial::error) << ARG
#define LOG_DATA_FATAL(ARG) BOOST_LOG_SEV(dataLogger::get(), boost::log::trivial::fatal) << ARG

} /* RESTClient */
