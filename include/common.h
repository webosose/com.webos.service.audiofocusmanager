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

#ifndef COMMON_H
#define COMMON_H

#include <lunaservice.h>
#include <map>
#include <string>
#include <list>
#include <pbnjson.hpp>

typedef struct RequestTypePolicyInfo
{
    int priority {-1};
    pbnjson::JValue incomingRequestInfo {pbnjson::Array()};
}REQUEST_TYPE_POLICY_INFO_T;

typedef struct AppInfo
{
    std::string appId;
    std::string requestType;
    std::string streamType;
}APP_INFO_T;

typedef struct SessionInfo
{
    std::list<APP_INFO_T> activeAppList;
    std::list<APP_INFO_T> pausedAppList;
}SESSION_INFO_T;

using RequestPolicyInfoMap = std::map<std::string, REQUEST_TYPE_POLICY_INFO_T>;
using SessionInfoMap = std::map<int, SESSION_INFO_T>;

struct CLSError : public LSError
{
    CLSError()
    {
        LSErrorInit(this);
    }
    void Print(const char * where, int line, GLogLevelFlags logLevel = G_LOG_LEVEL_WARNING);
    void Free()
    {
        LSErrorFree(this);
    }
};

typedef enum ResponseType
{
    eLSReply,
    eLSRespond
}RESPONSE_TYPE;

#endif
