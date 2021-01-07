/* @@@LICENSE
*
*      Copyright (c) 2020 - 2021 LG Electronics Company.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */

#ifndef LOG_H_
#define LOG_H_

#include <PmLogLib.h>
#include <glib.h>

//get Audio focus manager pm log context
PmLogContext getPmLogContext();

//set Audio focus manager pm log context
PmLogErr setPmLogContext(const char* logContextName);

//This is for PmLog implementation
extern PmLogContext audioFocusMgrLogContext;
#define INIT_KVCOUNT                0

#define PM_LOG_CRITICAL(msgid, kvcount, ...)  PmLogCritical(getPmLogContext(), msgid, kvcount, ##__VA_ARGS__)
#define PM_LOG_ERROR(msgid, kvcount, ...)     PmLogError(getPmLogContext(), msgid, kvcount, ##__VA_ARGS__)
#define PM_LOG_WARNING(msgid, kvcount, ...)   PmLogWarning(getPmLogContext(), msgid, kvcount, ##__VA_ARGS__)
#define PM_LOG_INFO(msgid, kvcount, ...)      PmLogInfo(getPmLogContext(), msgid, kvcount, ##__VA_ARGS__)
#define PM_LOG_DEBUG(...)                     PmLogDebug(getPmLogContext(), ##__VA_ARGS__)

//If log level is higher than DEBUG(lowest), you need to use Message ID.
//Start up and shutdown message ID's
#define MSGID_STARTUP                                                        "AUDIOFOCUSMANAGER_STARTUP"                    //Audio focus manager start up logs
#define MSGID_STUTDOWN                                                       "AUDIOFOCUSMANAGER_STUTDOWN"                   //Audio focus manager shutdown logs
#define MSGID_INIT                                                           "AUDIOFOCUSMANAGER_INIT"                       //Audio focus manager inialization logs
#define MSGID_CORE                                                           "AUDIOFOCUSMANAGER_CORE"                       //Audio focus manager core class logs
#define MSGID_PARSE_JSON                                                     "AUDIOFOCUSMANAGER_PARSE_JSON"                 //Audio focus manager Parse json
#define MSGID_MALFORMED_JSON                                                 "AUDIOFOCUSMANAGER_MALFORMED_JSON"             //Audio focus manager Malformed json error
#define MSGID_TEST_FAILED                                                    "AUDIOFOCUSMANAGER_UT_FAILED"                   //Audio focus manager Unittest failed
#define MSGID_INVALID_INPUT                                                  "INVALID_INPUT"                                 //Invalid input params received
#define MSGID_DATA_NULL                                                      "DATA_NULL"                                     //Null data received in pointer

/// Test macro that will make a critical log entry if the test fails
#define VERIFY(t) (G_LIKELY(t) || (PM_LOG_ERROR(MSGID_VERIFY_FAILED, INIT_KVCOUNT,\
                                                        "%s %s %d %s",   \
                                                        #t, __FILE__, __LINE__,  \
                                                        __FUNCTION__), false))

/// Test macro that will make a warning log entry if the test fails
#define CHECK(t) (G_LIKELY(t) || ((PM_LOG_ERROR(MSGID_VERIFY_FAILED, INIT_KVCOUNT,\
                                                        "%s %s %d %s",   \
                                                        #t, __FILE__, __LINE__,  \
                                                        __FUNCTION__), false))

/// Direct critical message to put in the logs with
// file & line number, with filtering of repeats
#define FAILURE(m) PM_LOG_ERROR(MSGID_VERIFY_FAILED, INIT_KVCOUNT,\
                                                        "%s %s %d %s", \
                                                        m, __FILE__, __LINE__, __FUNCTION__)


#endif // LOG_H_

