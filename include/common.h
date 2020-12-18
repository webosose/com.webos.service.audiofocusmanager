/* @@@LICENSE
*
*      Copyright (c) 2020 LG Electronics Company.
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

namespace common {
    enum class RequestType {
        INVALID = -1,
        GAIN,
        MUTE_LONG,
        MUTE_SHORT,
        MUTE_MUTUAL,
        CALL,
        TRANSIENT,
        MUTE_VOICE
    };
}

struct RequestTypePolicyInfo
{
    common::RequestType request {common::RequestType::INVALID};
    int priority {-1};
    std::string type;
};
struct AppInfo
{
    std::string appId;
    common::RequestType request;
};
struct SessionInfo
{
    std::list<AppInfo> activeAppList;
    std::list<AppInfo> pausedAppList;
    std::string sessionId;
};

using RequestPolicyInfoMap = std::map<common::RequestType, RequestTypePolicyInfo>;
using MapRequestNameToType = std::map<std::string, common::RequestType>;
using MapRequestTypeToName = std::map<common::RequestType, std::string>;
using ItMapRequestNameToType = std::map<std::string, common::RequestType>::iterator;
using SessionInfoMap = std::map<std::string, SessionInfo>;

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

#endif
