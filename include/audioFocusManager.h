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

#ifndef AudioFocusManager_H
#define AudioFocusManager_H

#include <string>
#include <iostream>
#include <list>
#include <map>
#include <glib.h>
#include "common.h"

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

struct LSHandle;
struct LSMessage;
struct LSPalmService;

class AudioFocusManager
{
public:
    ~AudioFocusManager(){};
    bool init(GMainLoop *);
    static bool _requestAudioControl(LSHandle *sh, LSMessage *message, void *data)
    {
        return ((AudioFocusManager *) data)->requestAudioControl(sh, message, NULL);
    }

    static bool _releaseAudioControl(LSHandle *sh, LSMessage *message, void *data)
    {
        return ((AudioFocusManager *) data)->releaseAudioControl(sh, message, NULL);
    }

    static bool _getStatus(LSHandle *sh, LSMessage *message, void *data)
    {
        return ((AudioFocusManager *) data)->getStatus(sh, message, NULL);
    }

    static AudioFocusManager *getInstance();
    void deleteInstance();
    bool signalTermCaught();

    void LSErrorPrintAndFree(LSError *ptrLSError);
    bool audioServiceRegister(std::string serviceName, GMainLoop *mainLoop, LSHandle **mServiceHandle);
    static LSHandle *mServiceHandle;

private:

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

    RequestPolicyInfoMap mAFRequestPolicyInfo;
    MapRequestNameToType mRequestNameToType;
    MapRequestTypeToName mRequestTypeToName;
    SessionInfoMap mSessionInfoMap;
    static AudioFocusManager *AFService;
    static LSMethod rootMethod[];

    AudioFocusManager();

    bool releaseAudioControl(LSHandle *sh, LSMessage *message, void *data);
    bool requestAudioControl(LSHandle *sh, LSMessage *message, void *data);
    bool getStatus(LSHandle *sh, LSMessage *message, void *data);

    void broadcastStatusToSubscribers();
    std::string getAudioFocusManagerStatusPayload();
    bool loadRequestPolicyJsonConfig();
    void printRequestPolicyJsonInfo();
    void createMapRequestNametoType();
    void createMapRequestTypetoName();
    bool checkGrantedAlready(LSHandle *sh, LSMessage *message, std::string applicationId, const std::string& sessionId, common::RequestType& requestType);
    bool checkFeasibility(const std::string& sessionId, common::RequestType requestType);
    bool updateCurrentAppStatus(const std::string& sessionId, common::RequestType newRequest);
    void updateSessionActiveAppList(const std::string& sessionId, const std::string appId, common::RequestType requestType);
    void updatePausedAppStatus(SessionInfo& sessionInfo, common::RequestType removedRequest);
    bool sendLostMessage();
    bool sendPauseMessage();
    bool signalToApp(const std::string& appId, const std::string& signalMessage);
    bool subscriptionUtility(const std::string& applicationId, LSHandle *serviceHandle, const char operation, const std::string& signalMessage);
    bool sendSignal(LSHandle *sh, LSMessage *message, const std::string& signalMessage, LSError *lserror);
    bool returnErrorText(LSHandle *sh, LSMessage *message, const std::string& errorText, int errorCode);
    bool unsubscribingApp(const std::string& appId);
    bool checkSubscription(const std::string& applicationId);
    bool broadcastLostToAll(LSHandle *serviceHandle);

};

#endif