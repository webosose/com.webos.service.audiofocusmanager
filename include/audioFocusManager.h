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

LSHandle *GetLSService();

#define REQUEST_TYPE_POLICY_CONFIG "audiofocuspolicy.json"
#define AF_API_GET_STATUS "/getStatus"
#define CONFIG_DIR_PATH "/etc/palm/audiofocusmanager"
#define AF_ERR_CODE_INVALID_SCHEMA (1)
#define AF_ERR_CODE_UNKNOWN_REQUEST (2)
#define AF_ERR_CODE_INTERNAL (3)

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

    static AudioFocusManager *getInstance();
    void deleteInstance();
    bool signalTermCaught();

    void LSErrorPrintAndFree(LSError *ptrLSError);
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

    void broadcastStatusToSubscribers();
    pbnjson::JValue getStatusPayload();
    bool loadRequestPolicyJsonConfig();
    void printRequestPolicyJsonInfo();
    bool checkGrantedAlready(LSHandle *sh, LSMessage *message, std::string applicationId, const std::string& sessionId, const std::string& requestType);
    bool checkFeasibility(const std::string& sessionId, const std::string& newRequestType);
    bool updateCurrentAppStatus(const std::string& sessionId, const std::string& newRequestType);
    void updateSessionActiveAppList(const std::string& sessionId, const std::string& appId, const std::string& requestType);
    void updatePausedAppStatus(SESSION_INFO_T& sessionInfo, const std::string& removedRequest);
    bool subscriptionUtility(const std::string& applicationId, LSHandle *serviceHandle, const char operation, const std::string& signalMessage);
    bool sendSignal(LSHandle *sh, LSMessage *message, const std::string& signalMessage, LSError *lserror);
    bool returnErrorText(LSHandle *sh, LSMessage *message, const std::string& errorText, int errorCode);
    bool unsubscribingApp(const std::string& appId);
    bool checkSubscription(const std::string& applicationId);
    bool signalToApp(const std::string& appId, const std::string& returnType);
};

#endif
