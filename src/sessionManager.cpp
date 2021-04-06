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

#include "audioFocusManager.h"

#if defined(WEBOS_SOC_AUTO)
bool AudioFocusManager::serviceStatusCallBack( LSHandle *sh, const char *serviceName, bool connected, void *ctx)
{
    PM_LOG_INFO(MSGID_SESSION_MANAGER, INIT_KVCOUNT,"serviceStatusCallBack recieved from %s, status =%d",serviceName, connected);
    AudioFocusManager *AFObj = AudioFocusManager::getInstance();
    std::string payload = "{\"subscribe\":true}";
    if (connected)
    {
        LSCall (GetLSService(), GET_SESSION_LIST, payload.c_str(), sessionListCallback, AFObj, nullptr, nullptr);
    }
    return true;

}

bool AudioFocusManager::sessionListCallback(LSHandle *lshandle, LSMessage *message, void *ctx)
{
    PM_LOG_INFO(MSGID_SESSION_MANAGER, INIT_KVCOUNT,"sessionListCallback recieved");
    AudioFocusManager *AFObj = AudioFocusManager::getInstance();
    AFObj->mSessionInfoMap.clear();
    AFObj->readSessionInfo(message);
    return true;
}

void AudioFocusManager::readSessionInfo(LSMessage *message)
{
    LSMessageJsonParser msg(message, SCHEMA_ANY);
    if (!msg.parse(__func__))
        return;
    bool returnValue = false;
    msg.get("returnValue", returnValue);
    if (returnValue)
    {
        pbnjson::JValue payload = msg.get();
        pbnjson::JValue sessionInfo = payload["sessions"];
        if (!sessionInfo.isArray())
        {
            PM_LOG_ERROR(MSGID_SESSION_MANAGER, INIT_KVCOUNT,\
                "%s sessionList is not an array", __FUNCTION__);
            return;
        }
        else
        {
            std::string deviceType;
            std::string deviceSetId;
            std::string sessionId;
            for (const auto& items : sessionInfo.items())
            {
                if (items.hasKey("deviceSetInfo"))
                {
                    SESSION_INFO_T stSessionInfo;
                    pbnjson::JValue deviceSetInfo = items["deviceSetInfo"];
                    stSessionInfo.displayId = deviceSetInfo["displayId"].asNumber<int>();
                    if (deviceSetInfo["deviceType"].asString(deviceType) == CONV_OK)
                        stSessionInfo.deviceType = deviceType;
                    if (deviceSetInfo["deviceSetId"].asString(deviceSetId) == CONV_OK)
                        stSessionInfo.deviceSetId = deviceSetId;
                    if (items["sessionId"].asString(sessionId) == CONV_OK)
                        mSessionInfoMap.insert(std::pair<std::string, SESSION_INFO_T>(sessionId, stSessionInfo));
                }
                else
                    PM_LOG_ERROR(MSGID_SESSION_MANAGER, INIT_KVCOUNT, \
                        "deviceSetInfo key is not present");
            }
        }
    }
    else
        PM_LOG_ERROR(MSGID_SESSION_MANAGER, INIT_KVCOUNT, \
            "returnValue is false");
    printSessionInfo();
}


void AudioFocusManager::printSessionInfo()
{
    PM_LOG_INFO(MSGID_SESSION_MANAGER, INIT_KVCOUNT,\
        "SessionManager::printSessionInfo: ");
    for (auto &elements: mSessionInfoMap)
    {
         PM_LOG_INFO(MSGID_SESSION_MANAGER, INIT_KVCOUNT,\
             "sessionId:%s displayId:%d deviceType:%s deviceSetId:%s",\
             elements.first.c_str(), elements.second.displayId,\
             elements.second.deviceType.c_str(), elements.second.deviceSetId.c_str());
    }
}


int AudioFocusManager::getSessionDisplayId(const std::string &sessionInfo)
{
    PM_LOG_INFO(MSGID_SESSION_MANAGER, INIT_KVCOUNT, "getSessionDisplayId:%s", sessionInfo.c_str());
    int displayId = -1;
    if (HOST_SESSION == sessionInfo)
        displayId = DISPLAY_ID_0;
    else
    {
        PM_LOG_INFO(MSGID_SESSION_MANAGER, INIT_KVCOUNT, "sessionCount:%d", (int)mSessionInfoMap.size());
        itMapSessionInfo it = mSessionInfoMap.find(sessionInfo);
        if (it != mSessionInfoMap.end())
        {
            PM_LOG_INFO(MSGID_SESSION_MANAGER, INIT_KVCOUNT, "found session for deviceSetId:%s",\
                it->second.deviceSetId.c_str());
            if (AVN_SESSION == it->second.deviceSetId)
                displayId = DISPLAY_ID_0;
            if (RSE_LEFT_SESSION == it->second.deviceSetId)
                displayId = DISPLAY_ID_1;
            if (RSE_RIGHT_SESSION == it->second.deviceSetId)
                displayId = DISPLAY_ID_2;
        }
    }
    return displayId;
}
#endif