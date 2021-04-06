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

#ifndef AudioFocusManager_H
#define AudioFocusManager_H

#include <string>
#include <iostream>
#include <list>
#include <map>
#include <glib.h>
#include "common.h"
#include <sys/time.h>
#include <pbnjson.hpp>
#include <lunaservice.h>
#include <sstream>
#include "messageUtils.h"
#include "log.h"
#include "utils.h"

LSHandle *GetLSService();

#define REQUEST_TYPE_POLICY_CONFIG "audiofocuspolicy.json"
#define AF_API_GET_STATUS "/getStatus"
#define AF_API_REQUEST_FOCUS "requestFocus"
#define CONFIG_DIR_PATH "/etc/palm/audiofocusmanager"

#define AF_ERR_CODE_INVALID_SCHEMA 1
#define AF_ERR_CODE_UNKNOWN_REQUEST 2
#define AF_ERR_CODE_INTERNAL 3
#define AF_ERR_CODE_INVALID_DISPLAY_ID 4

#define DISPLAY_ID_0 0
#define DISPLAY_ID_1 1
#define DISPLAY_ID_2 2

#if defined(WEBOS_SOC_AUTO)
#define UNKNOWN_SESSION_ID -1
#define HOST_SESSION        "host"
#define AVN_SESSION         "AVN"
#define RSE_LEFT_SESSION    "RSE-L"
#define RSE_RIGHT_SESSION   "RSE-R"
#define MAX_SESSIONS        3

#define ACCOUNT_SERVICE     "com.webos.service.account"
#define GET_SESSION_LIST    "luna://com.webos.service.account/getSessions"
#endif

#define LSERROR_CHECK_AND_PRINT(ret)\
    do {              \
    if(ret == false)  \
    {                 \
        LSErrorPrintAndFree(&mLSError);\
        return false; \
    }                 \
    } while (0)

class AudioFocusManager
{
public:
    ~AudioFocusManager(){};
    bool init(GMainLoop *);
    static bool _requestFocus(LSHandle *sh, LSMessage *message, void *data)
    {
        return ((AudioFocusManager *) data)->requestFocus(sh, message, NULL);
    }

    static bool _releaseFocus(LSHandle *sh, LSMessage *message, void *data)
    {
        return ((AudioFocusManager *) data)->releaseFocus(sh, message, NULL);
    }

    static bool _getStatus(LSHandle *sh, LSMessage *message, void *data)
    {
        return ((AudioFocusManager *) data)->getStatus(sh, message, NULL);
    }
    static bool _cancelFunction(LSHandle *sh, LSMessage *message, void *data)
    {
       return ((AudioFocusManager *) data)->cancelFunction(sh, message, NULL);
    }

    static AudioFocusManager *getInstance();
    static void deleteInstance();
    static void loadAudioFocusManager();

    bool signalTermCaught();
    bool audioFunctionsRegister(std::string srvcname, GMainLoop *mainLoop, LSHandle *msvcHandle);
    static LSHandle *mServiceHandle;
#if defined(WEBOS_SOC_AUTO)
    static bool sessionListCallback(LSHandle *lshandle, LSMessage *message, void *ctx);
    static bool serviceStatusCallBack( LSHandle *sh, const char *serviceName, bool connected, void *ctx);
#endif
private:

    RequestPolicyInfoMap mAFRequestPolicyInfo;
    DisplayInfoMap mDisplayInfoMap;
    static AudioFocusManager *AFService;
    static LSMethod rootMethod[];
#if defined(WEBOS_SOC_AUTO)
    mapSessionInfo mSessionInfoMap;
    void readSessionInfo(LSMessage *message);
    void printSessionInfo();
    int getSessionDisplayId(const std::string &sessionInfo);
#endif
    AudioFocusManager();

    bool releaseFocus(LSHandle *sh, LSMessage *message, void *data);
    bool requestFocus(LSHandle *sh, LSMessage *message, void *data);
    bool getStatus(LSHandle *sh, LSMessage *message, void *data);
    bool cancelFunction(LSHandle *sh, LSMessage *message, void *data);

    bool validateDisplayId(int displayId);
    void broadcastStatusToSubscribers(int displayId);
    pbnjson::JValue getStatusPayload(const int& displayId);
    bool loadRequestPolicyJsonConfig();
    void printRequestPolicyJsonInfo();
    void sendApplicationResponse(LSHandle *serviceHandle, LSMessage *message, const std::string& payload);
    bool checkGrantedAlready(LSHandle *sh, LSMessage *message, std::string applicationId, const int& displayId, const std::string& requestType);
    bool checkFeasibility(const int& displayId, const std::string& newRequestType);
    void updateDisplayActiveAppList(const int& displayId, const std::string& appId, const std::string& requestType, const std::string& streamType);
    void manageAppSubscription(const std::string& applicationId, const std::string& payload, const char operation);
    bool checkIncomingPair(const std::string& newRequestType, const std::list<APP_INFO_T>& appList);
    bool pausedAppToActive(DISPLAY_INFO_T& displayInfo, const std::string& removedRequest);
    bool isIncomingPairRequestTypeActive(const std::string& requestType, const DISPLAY_INFO_T& displayInfo);
    std::string getFocusPolicyType(const std::string& newRequestType, const pbnjson::JValue& incomingRequestInfo);
};

#endif
