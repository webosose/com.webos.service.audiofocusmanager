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
#define AF_ERR_CODE_INVALID_SESSION_ID 4

#define DISPLAY_ID_0 0
#define DISPLAY_ID_1 1
#define DISPLAY_ID_2 2

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

private:

    RequestPolicyInfoMap mAFRequestPolicyInfo;
    SessionInfoMap mSessionInfoMap;
    static AudioFocusManager *AFService;
    static LSMethod rootMethod[];

    AudioFocusManager();

    bool releaseFocus(LSHandle *sh, LSMessage *message, void *data);
    bool requestFocus(LSHandle *sh, LSMessage *message, void *data);
    bool getStatus(LSHandle *sh, LSMessage *message, void *data);
    bool cancelFunction(LSHandle *sh, LSMessage *message, void *data);

    bool validateSessionId(int sessionId);
    void broadcastStatusToSubscribers(int sessionId);
    pbnjson::JValue getStatusPayload(const int& sessionId);
    bool loadRequestPolicyJsonConfig();
    void printRequestPolicyJsonInfo();
    void sendApplicationResponse(LSHandle *serviceHandle, LSMessage *message, const std::string& payload);
    bool checkGrantedAlready(LSHandle *sh, LSMessage *message, std::string applicationId, const int& sessionId, const std::string& requestType);
    bool checkFeasibility(const int& sessionId, const std::string& newRequestType);
    void updateSessionActiveAppList(const int& sessionId, const std::string& appId, const std::string& requestType);
    void manageAppSubscription(const std::string& applicationId, const std::string& payload, const char operation);
    bool checkIncomingPair(const std::string& newRequestType, const std::list<APP_INFO_T>& appList);
    bool pausedAppToActive(SESSION_INFO_T& sessionInfo, const std::string& removedRequest);
    bool isIncomingPairRequestTypeActive(const std::string& requestType, const SESSION_INFO_T& sessionInfo);
    std::string getFocusPolicyType(const std::string& newRequestType, const pbnjson::JValue& incomingRequestInfo);
};

#endif
