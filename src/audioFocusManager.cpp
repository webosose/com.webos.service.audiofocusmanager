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

#include <audioFocusManager.h>

LSHandle *AudioFocusManager::mServiceHandle;

LSMethod AudioFocusManager::rootMethod[] = {
    {"requestFocus", AudioFocusManager::_requestFocus},
    {"releaseFocus", AudioFocusManager::_releaseFocus},
    {"getStatus", AudioFocusManager::_getStatus},
    {0, 0}
};

AudioFocusManager *AudioFocusManager::AFService = NULL;

AudioFocusManager::AudioFocusManager()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "AudioFocusManager Constructor invoked");
}

/*
Functionality of this method:
->This function will create the object for AudioFocusManager if not present and returns the pointer.
->If the object is already created then it will just return the pointer.
*/
void AudioFocusManager::loadAudioFocusManager()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "loadAudioFocusManager");
    if(NULL == AFService)
    {
        AFService = new(std::nothrow) AudioFocusManager();
    }
}

/*
Functionality of this method:
->This function will return the object for AudioFocusManager.
*/
AudioFocusManager *AudioFocusManager::getInstance()
{
    return AFService;
}

/*
Functionality of this method:
->This function will delete the object for AudioFocusManager if present.
->If the object is not present then it will do nothing.
*/
void AudioFocusManager::deleteInstance()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "deleteInstance");
    if(AFService)
    {
        delete AFService;
        AFService = NULL;
    }
    return;
}

/*
Functionality of this method:
->Initializes the service registration.
*/
bool AudioFocusManager::init(GMainLoop *mainLoop)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"init");

    if(audioFunctionsRegister("com.webos.service.audiofocusmanager", mainLoop, GetLSService()) == false)
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "com.webos.service.audiofocusmanager service registration failed");
        return false;
    }

    if(loadRequestPolicyJsonConfig() == false)
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "Failed to parse RequestPolicy Json config");
        return false;
    }

    printRequestPolicyJsonInfo();
    return true;
}

/*
 * Functionality of this method:
 * ->Load the RequestPolicy JSON config and populate internal structure for request info
 */
bool AudioFocusManager::loadRequestPolicyJsonConfig()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"loadRequestPolicyJsonConfig");

    std::stringstream jsonFilePath;
    jsonFilePath << CONFIG_DIR_PATH << "/" << REQUEST_TYPE_POLICY_CONFIG;
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "Loading request types policy info from json file %s",\
                    jsonFilePath.str().c_str());

    pbnjson::JValue fileJsonRequestPolicyConfig = pbnjson::JDomParser::fromFile(jsonFilePath.str().c_str(),\
            pbnjson::JSchema::AllSchema());
    if (!fileJsonRequestPolicyConfig.isValid() || !fileJsonRequestPolicyConfig.isObject())
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "Failed to parse json config file, using defaults. File: %s", jsonFilePath.str().c_str());
        return false;
    }

    pbnjson::JValue policyInfo = fileJsonRequestPolicyConfig["requestType"];
    if (!policyInfo.isArray())
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "request policyInfo is not an array");
        return false;
    }

    for (const pbnjson::JValue& elements : policyInfo.items())
    {
        REQUEST_TYPE_POLICY_INFO_T stPolicyInfo;
        std::string requestType;
        int priority;
        if (elements["request"].asString(requestType) == CONV_OK)
        {
            stPolicyInfo.priority = elements["priority"].asNumber<int>();
            pbnjson::JValue incomingRequestInfo = elements["incoming"];
            if (incomingRequestInfo.isArray())
                stPolicyInfo.incomingRequestInfo = incomingRequestInfo;
            mAFRequestPolicyInfo[requestType] = stPolicyInfo;
        }
        else
        {
            PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, \
                "loadRequestPolicyJsonConfig: Invalid request type entry in config file, skipping");
            continue;
        }
    }
    return true;
}

void AudioFocusManager::printRequestPolicyJsonInfo()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"printRequestPolicyJsonInfo");
    for (const auto& it : mAFRequestPolicyInfo)
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "RequestType: %s  Priority: %d incomingRequestInfo: %s", it.first.c_str(), \
            it.second.priority, it.second.incomingRequestInfo.stringify().c_str());
}

/*
Functionality of this method:
->Registers the service with lunabus.
->Registers the methods with lunabus as public methods.
*/
bool AudioFocusManager::audioFunctionsRegister(std::string serviceName, GMainLoop *mainLoop, LSHandle *mServiceHandle)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"audioFunctionsRegister");
    bool bRetVal;
    CLSError mLSError;
    // add root category
    if (!LSRegisterCategory (mServiceHandle, "/",
                                 rootMethod, NULL, NULL, &mLSError))
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "API registration failed");
        mLSError.Print(__FUNCTION__,__LINE__);
        return false;
    }
    if (!LSCategorySetData(mServiceHandle, "/", this, &mLSError))
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "Setting userdata for APIs failed");
        mLSError.Print(__FUNCTION__,__LINE__);
        return false;
    }
    if (!LSSubscriptionSetCancelFunction(mServiceHandle, AudioFocusManager::_cancelFunction, this, &mLSError))
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "Setting cancelfunction for APIs failed");
        mLSError.Print(__FUNCTION__,__LINE__);
        return false;
    }
    return true;
}

bool AudioFocusManager::cancelFunction(LSHandle *sh, LSMessage *message, void *data)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "Subscription cancelled");
    const char* method = LSMessageGetMethod(message);
    LSMessageJsonParser msg(message, SCHEMA_ANY);
    if (!msg.parse(__FUNCTION__,sh))
       return true;
    const char* appId = LSMessageGetApplicationID(message);
    if (appId == NULL)
    {
        appId = LSMessageGetSenderServiceName(message);
    }
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "Subscription cancelled from app %s", appId);
    if(strcmp(method, AF_API_REQUEST_FOCUS) == 0)
    {
        int sessionId = -1;
        msg.get("sessionId", sessionId);
        auto itSession = mSessionInfoMap.find(sessionId);
        if (itSession == mSessionInfoMap.end())
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: New session entry for:%d Request feasible", sessionId);
            return true;
        }
        SESSION_INFO_T& sessionInfo = itSession->second;
        for (auto itPaused = sessionInfo.pausedAppList.begin(); \
            itPaused != sessionInfo.pausedAppList.end(); itPaused++)
        {
            if (appId == itPaused->appId)
            {
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "Paused app Killed, remove from list %s Request type: %s", \
                    appId, itPaused->requestType.c_str());
                sessionInfo.pausedAppList.erase(itPaused--);
                broadcastStatusToSubscribers(sessionId);
                return true;
            }
        }
        for (auto itActive = sessionInfo.activeAppList.begin(); itActive != sessionInfo.activeAppList.end(); itActive++)
        {
            if (itActive->appId == appId)
            {
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"Active app Killed: Removing appId: %s Request type: %s", \
                    appId, itActive->requestType.c_str());
                std::string requestType = itActive->requestType;
                sessionInfo.activeAppList.erase(itActive--);
                updatePausedAppStatus(sessionInfo, requestType);
                broadcastStatusToSubscribers(sessionId);
                return true;
            }
        }
    }
    return true;
}

/*
Functionality of this method:
->Parses the payload.
->If resource is free, it will grant the resource to the requested application.
->If resource is not free, it invokes the appropriate method based on the request type.
*/
bool AudioFocusManager::requestFocus(LSHandle *sh, LSMessage *message, void *data)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"requestFocus");

    LSMessageJsonParser msg(message, STRICT_SCHEMA(PROPS_3 (PROP(requestType, string), PROP(sessionId, integer),
                                    PROP(subscribe, boolean)) REQUIRED_3(requestType, sessionId, subscribe)));

    if (!msg.parse(__FUNCTION__,sh))
       return true;

    int sessionId = -1;
    std::string requestName;
    std::string reply;
    bool subscription;

    msg.get("sessionId", sessionId);
    msg.get("requestType", requestName);
    msg.get("subscribe", subscription);

    if (!validateSessionId(sessionId))
    {
        reply = STANDARD_JSON_ERROR(AF_ERR_CODE_INVALID_SESSION_ID, "Invalid sessionId");
        LSMessageResponse(sh, message, reply.c_str(), eLSReply, false);
        return true;
    }
    const char* appId = LSMessageGetApplicationID(message);
    if (appId == NULL)
    {
        appId = LSMessageGetSenderServiceName(message);
        if (appId == NULL)
        {
            reply = STANDARD_JSON_ERROR(AF_ERR_CODE_INTERNAL, "appId received as NULL");
            LSMessageResponse(sh, message, reply.c_str(), eLSReply, false);
            return true;
        }
    }
    const auto& it = mAFRequestPolicyInfo.find(requestName);
    if (it != mAFRequestPolicyInfo.end())
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "Valid request type received");
    else
    {
        reply = STANDARD_JSON_ERROR(AF_ERR_CODE_UNKNOWN_REQUEST, "Invalid Request Type");
        LSMessageResponse(sh, message, reply.c_str(), eLSReply, false);
        return true;
    }

    if (!subscription)
    {
        reply = STANDARD_JSON_ERROR(AF_ERR_CODE_INTERNAL, "Subscription should be true");
        LSMessageResponse(sh, message, reply.c_str(), eLSReply, false);
        return true;
    }
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "requestFocus: sessionId: %d requestType: %s appId %s", \
        sessionId, requestName.c_str(), appId);
    if (checkGrantedAlready(sh, message, appId, sessionId, requestName))
        return true;
    if (!checkFeasibility(sessionId, requestName))
    {
        sendApplicationResponse(sh, message, "AF_CANNOTBEGRANTED");
        return true;
    }
    if (!updateCurrentAppStatus(sessionId, requestName))
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "requestFocus: updateCurrentAppStatus Failed");
    sendApplicationResponse(sh, message, "AF_GRANTED");
    if (LSMessageIsSubscription(message))
        LSSubscriptionAdd(sh, "AFSubscriptionList", message, NULL);
    updateSessionActiveAppList(sessionId, appId, requestName);
    broadcastStatusToSubscribers(sessionId);
    return true;
}
/*
Functionality of this method:
Validating the session Id by matching it with the display Id
*/
bool AudioFocusManager::validateSessionId(int sessionId)
{
    if(sessionId == DISPLAY_ID_0 || sessionId == DISPLAY_ID_1 || sessionId == DISPLAY_ID_2)
        return true;
    else
        return false;
}

/*
Functionality of this method:
->Checks whether the incoming request is duplicate request or not.
->If it is a duplicate request sends AF_GRANTEDALREADY event to the app.
*/
bool AudioFocusManager::checkGrantedAlready(LSHandle *sh, LSMessage *message, std::string applicationId,\
    const int& sessionId, const std::string& requestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkGrantedAlready for appId:%s sessionId:%d requestType:%s",\
        applicationId.c_str(), sessionId, requestType.c_str());
    auto it = mSessionInfoMap.find(sessionId);
    if (it == mSessionInfoMap.end())
    {
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"Request from a new session %d", sessionId);
        return false;
    }
    SESSION_INFO_T& sessionInfo = it->second;
    for (auto itPaused = sessionInfo.pausedAppList.begin(); \
            itPaused != sessionInfo.pausedAppList.end(); itPaused++)
    {
        if (itPaused->requestType == requestType && \
                itPaused->appId == applicationId)
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkGrantedAlready: AF_GRANTEDALREADY in paused list:%s", applicationId.c_str());
            sendApplicationResponse(sh, message, "AF_GRANTEDALREADY");
            return true;
        }
    }
    for (auto itActive = sessionInfo.activeAppList.begin(); \
            itActive != sessionInfo.activeAppList.end(); itActive++)
    {
        if (itActive->requestType == requestType && \
                itActive->appId == applicationId)
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkGrantedAlready: AF_GRANTEDALREADY in active list%s", applicationId.c_str());
            sendApplicationResponse(sh, message, "AF_GRANTEDALREADY");
            return true;
        }
    }
    return false;
}

/*Functionality of this method:
 * To check if active request types in the requesting session has any request which will not grant the new request type*/
bool AudioFocusManager::checkFeasibility(const int& sessionId, const std::string& newRequestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility for sessionId:%d newRequestType:%s",\
        sessionId, newRequestType.c_str());
    auto itNewRequestPolicy = mAFRequestPolicyInfo.find(newRequestType);
    if (itNewRequestPolicy == mAFRequestPolicyInfo.end())
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: New request type policy cannot be found");
        return false;
    }
    REQUEST_TYPE_POLICY_INFO_T& newRequestPolicy = itNewRequestPolicy->second;
    auto itSession = mSessionInfoMap.find(sessionId);
    if (itSession == mSessionInfoMap.end())
    {
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: New session entry for:%d Request feasible", sessionId);
        return true;
    }
    SESSION_INFO_T& curSessionInfo = itSession->second;
    //Check feasiblity in activeAppList
    for (const auto& itActive : curSessionInfo.activeAppList)
    {
        std::string activeRequest = itActive.requestType;
        auto itActiveRequestPolicy = mAFRequestPolicyInfo.find(activeRequest);
        if (itActiveRequestPolicy == mAFRequestPolicyInfo.end())
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: Invalid request type entry. Skipping...");
            continue;

        }
        REQUEST_TYPE_POLICY_INFO_T& activeRequestPolicy = itActiveRequestPolicy->second;
        //TODO
        /*if (activeRequestPolicy.priority < newRequestPolicy.priority && \
                activeRequestPolicy.type != "mix")
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: newrequestType: %s cannot be granted due to active request: %s", \
                newRequestType.c_str(), activeRequest.c_str());
            return false;
        }*/
    }
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: New requestType: %s shall be approved", \
        newRequestType.c_str());
    return true;
}

bool AudioFocusManager::updateCurrentAppStatus(const int& sessionId, const std::string& newRequestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus for sessionId:%d requestType:%s",\
        sessionId, newRequestType.c_str());
    auto itNewRequestPolicy = mAFRequestPolicyInfo.find(newRequestType);
    if (itNewRequestPolicy == mAFRequestPolicyInfo.end())
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: New request policy cannot be found");
        return false;
    }

    REQUEST_TYPE_POLICY_INFO_T& newRequestPolicy = itNewRequestPolicy->second;
    auto itSession = mSessionInfoMap.find(sessionId);
    if (itSession == mSessionInfoMap.end())
    {
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "updateCurrentAppStatus: new session requested: %d No need to update app status", \
            sessionId);
        return true;
    }
    SESSION_INFO_T& curSessionInfo = itSession->second;
    //Update active app list based on new request
    for (auto itActive = curSessionInfo.activeAppList.begin(); itActive != curSessionInfo.activeAppList.end(); itActive++)
    {
        const auto& itActiveRequestPolicy = mAFRequestPolicyInfo.find(itActive->requestType);
        if (itActiveRequestPolicy == mAFRequestPolicyInfo.end())
        {
            PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Invalid requestType in active list for %d. Skipping", \
                    sessionId);
            continue;
        }
        REQUEST_TYPE_POLICY_INFO_T& activeRequestPolicy = itActiveRequestPolicy->second;
        //TODO
        /*if (activeRequestPolicy.priority >= newRequestPolicy.priority)
        {
            if (newRequestPolicy.type == "short" && \
                   (activeRequestPolicy.type == "long" || activeRequestPolicy.type == "mix"))
            {
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: send AF_PAUSE to %s", \
                    itActive->appId.c_str());
                manageAppSubscription(itActive->appId, "AF_PAUSE", 's');
                curSessionInfo.pausedAppList.push_back(*itActive);
                curSessionInfo.activeAppList.erase(itActive--);
            }
            else if (newRequestPolicy.type != "mix")
            {
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: send AF_LOST to %s", \
                        itActive->appId.c_str());
                manageAppSubscription(itActive->appId, "AF_LOST", 'n');
                curSessionInfo.activeAppList.erase(itActive--);
            }
            else
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: App can mix and play %s", \
                    itActive->appId.c_str());
        }*/
    }
    //TODO
    //Update paused apps list based on new request
    /*for (auto itPaused = curSessionInfo.pausedAppList.begin(); itPaused != curSessionInfo.pausedAppList.end(); itPaused++)
    {
        const auto& itPausedRequestPolicy = mAFRequestPolicyInfo.find(itPaused->requestType);
        if (itPausedRequestPolicy == mAFRequestPolicyInfo.end())
        {
            PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Invalid requestType in paused list for %d. Skipping", \
                    sessionId);
            continue;
        }
        if (newRequestPolicy.type == "long")
        {
            manageAppSubscription(itPaused->appId, "AF_LOST", 'n');
            curSessionInfo.pausedAppList.erase(itPaused--);
        }
    }*/
    return true;
}

/*Functionality of this methos:
 * -> Update the session active app list if session already present
 *  ->Create new session Info and update active app list
 */
void AudioFocusManager::updateSessionActiveAppList(const int& sessionId, const std::string& appId, const std::string& requestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateSessionAppList: sessionId: %d", sessionId);
    APP_INFO_T newAppInfo;
    newAppInfo.appId = appId;
    newAppInfo.requestType = requestType;
    auto itSession = mSessionInfoMap.find(sessionId);
    if (itSession == mSessionInfoMap.end())
    {
        SESSION_INFO_T newSessionInfo;
        newSessionInfo.activeAppList.push_back(newAppInfo);
        mSessionInfoMap[sessionId] = newSessionInfo;
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateSessionActiveAppList: new session details added. Session: %d", \
            sessionId);
        return;
    }
    SESSION_INFO_T& sessionInfo = itSession->second;
    sessionInfo.activeAppList.push_back(newAppInfo);
}

/*
Functionality of this method:
->This will unsubscribe the app and removes its entry.
->Sends resume event to the application which is in paused state if there are any such applications.
*/
bool AudioFocusManager::releaseFocus(LSHandle *sh, LSMessage *message, void *data)
{
    LSMessageJsonParser msg(message, STRICT_SCHEMA(PROPS_1(PROP(sessionId,integer)) REQUIRED_1(sessionId)));
    if (!msg.parse(__FUNCTION__, sh))
        return true;
    int sessionId = -1;
    std::string reply;
    msg.get("sessionId", sessionId);

    if (!validateSessionId(sessionId))
    {
        reply = STANDARD_JSON_ERROR(AF_ERR_CODE_INVALID_SESSION_ID, "Invalid sessionId");
        LSMessageResponse(sh, message, reply.c_str(), eLSReply, false);
        return true;
    }
    const char* appId = LSMessageGetApplicationID(message);
    if (appId == NULL)
    {
        appId = LSMessageGetSenderServiceName(message);
        if (appId == NULL)
        {
            reply = STANDARD_JSON_ERROR(AF_ERR_CODE_INTERNAL, "Internal error");
            LSMessageResponse(sh, message, reply.c_str(), eLSReply, false);
            return true;
        }
    }
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"releaseFocus: sessionId: %d appId: %s", sessionId, appId);
    auto itSession = mSessionInfoMap.find(sessionId);
    if (itSession == mSessionInfoMap.end())
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"releaseFocus: Session ID cannot be find");
        reply = STANDARD_JSON_ERROR(AF_ERR_CODE_INTERNAL, "Session Id not found");
        LSMessageResponse(sh, message, reply.c_str(), eLSReply, false);
        return true;
    }
    SESSION_INFO_T& curSessionInfo = itSession->second;
    for (auto itPaused = curSessionInfo.pausedAppList.begin(); itPaused != curSessionInfo.pausedAppList.end(); itPaused++)
    {
        if (itPaused->appId == appId)
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "releaseFocus: Removing appId: %s Request type: %s", \
                appId, itPaused->requestType.c_str());
            manageAppSubscription(appId, "AF_RELEASED", 'r');
            curSessionInfo.pausedAppList.erase(itPaused--);
            broadcastStatusToSubscribers(sessionId);
            sendApplicationResponse(sh, message, "AF_SUCCESSFULLY_RELEASED");
            return true;
        }
    }

    for (auto itActive = curSessionInfo.activeAppList.begin(); itActive != curSessionInfo.activeAppList.end(); itActive++)
    {
        if (itActive->appId == appId)
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"releaseFocus: Removing appId: %s Request type: %s", \
                appId, itActive->requestType.c_str());
            manageAppSubscription(appId, "AF_RELEASED", 'r');
            std::string requestType = itActive->requestType;
            curSessionInfo.activeAppList.erase(itActive--);
            updatePausedAppStatus(curSessionInfo, requestType);
            broadcastStatusToSubscribers(sessionId);
            sendApplicationResponse(sh, message, "AF_SUCCESSFULLY_RELEASED");
            return true;
        }
    }

    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "releaseFocus: appId: %s cannot be found in session: %d" , \
        appId, sessionId);
    reply = STANDARD_JSON_ERROR(AF_ERR_CODE_INTERNAL, "Application not registered");
    LSMessageResponse(sh, message, reply.c_str(), eLSReply, false);
    return true;
}

/*Functionality of this method:
 * To update the paused app list in the session based on the removed request type policy
 */
void AudioFocusManager::updatePausedAppStatus(SESSION_INFO_T& sessionInfo, const std::string& removedRequest)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "updatePausedAppStatus requestType:%s", removedRequest.c_str());
    const auto& requestInfoIt = mAFRequestPolicyInfo.find(removedRequest);
    int mixtypeActive = 0;
    if(requestInfoIt == mAFRequestPolicyInfo.end())
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "updatePausedAppStatus: unknown request type");
        return;
    }
    //TODO
    /*if (requestInfoIt->second.type != "short")
    {
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "updatePausedAppStatus: request type is not short. Nothing to update");
        return;
    }
    for( auto activeApp : sessionInfo.activeAppList)
    {
        auto activeRequestInfo = mAFRequestPolicyInfo.find(activeApp.requestType);
        if (activeRequestInfo == mAFRequestPolicyInfo.end())
        {
            PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "updatePausedAppStatus: unknown request type in active app");
            continue;
        }
        if (activeRequestInfo->second.type == "short")
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "updatePausedAppStatus: other short streams active. Nothing to update");
            return;
        }
    }*/
    //If any app is present in pause app list and active app list is empty,
    //grant focus to paused app,
    if (sessionInfo.activeAppList.empty() && !sessionInfo.pausedAppList.empty())
        pausedAppToActive(sessionInfo);
    return;
}

bool AudioFocusManager::pausedAppToActive(SESSION_INFO_T& sessionInfo)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "pausedAppToActive");
    if (sessionInfo.pausedAppList.size() == 1)
    {
        auto itPaused = sessionInfo.pausedAppList.back();
        sessionInfo.activeAppList.push_back(itPaused);
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "pausedAppToActive: send AF_GRANETD to %s", itPaused.appId.c_str());
        manageAppSubscription(itPaused.appId, "AF_GRANTED", 's');
        sessionInfo.pausedAppList.pop_back();
    }
    else
    {
        auto highPriorityApp = sessionInfo.pausedAppList.end();
        for (auto itPaused = sessionInfo.pausedAppList.begin(); \
            itPaused != sessionInfo.pausedAppList.end(); itPaused++)
        {
            auto it = mAFRequestPolicyInfo.find(itPaused->requestType);
            auto highPriorityReqType = mAFRequestPolicyInfo.find(highPriorityApp->requestType);
            PM_LOG_DEBUG("pausedAppToActive: %d, for loop priority = %d", it->second.priority, highPriorityReqType->second.priority);
            if (highPriorityReqType->second.priority > it->second.priority)
            {
                highPriorityApp = itPaused;
            }
        }
        sessionInfo.activeAppList.push_back(*highPriorityApp);
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "pausedAppToActive: send AF_GRANETD to %s", highPriorityApp->appId.c_str());
        manageAppSubscription(highPriorityApp->appId, "AF_GRANTED", 's');
        sessionInfo.pausedAppList.erase(highPriorityApp);
        auto it = mAFRequestPolicyInfo.find(highPriorityApp->requestType);
        //TODO
        /*if (it->second.type == "mix" && !sessionInfo.pausedAppList.empty())
        {
            auto highPriorityApp = sessionInfo.pausedAppList.end();
            for (auto itPaused = sessionInfo.pausedAppList.begin(); \
                itPaused != sessionInfo.pausedAppList.end(); itPaused++)
            {
                auto it = mAFRequestPolicyInfo.find(itPaused->requestType);
                auto highPriorityReqType = mAFRequestPolicyInfo.find(highPriorityApp->requestType);
                PM_LOG_DEBUG("pausedAppToActive: %d, for loop priority = %d", it->second.priority, highPriorityReqType->second.priority);
                if (highPriorityReqType->second.priority > it->second.priority)
                {
                    highPriorityApp = itPaused;
                }
            }
            sessionInfo.activeAppList.push_back(*highPriorityApp);
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "pausedAppToActive: send AF_GRANETD to %s", highPriorityApp->appId.c_str());
            manageAppSubscription(highPriorityApp->appId, "AF_GRANTED", 's');
            sessionInfo.pausedAppList.erase(highPriorityApp);
        }*/
    }
    return true;
}

/*
Functionality of this method:
->This will return the current status of granted request types in AudioFocusManager for each session
*/
bool AudioFocusManager::getStatus(LSHandle *sh, LSMessage *message, void *data)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"getStatus");
    CLSError lserror;
    pbnjson::JValue jsonObject = pbnjson::JObject();
    std::string reply;
    LSMessageJsonParser msg(message, STRICT_SCHEMA(PROPS_2(PROP(subscribe, boolean), PROP(sessionId, integer)) REQUIRED_1(sessionId)));

    if (!msg.parse(__FUNCTION__, sh))
        return true;
    bool subscription;
    int sessionId = -1;
    msg.get("sessionId",sessionId);
    if (!validateSessionId(sessionId))
    {
        reply = STANDARD_JSON_ERROR(AF_ERR_CODE_INVALID_SESSION_ID, "Invalid sessionId");
        LSMessageResponse(sh, message, reply.c_str(), eLSReply, false);
        return true;
    }
    msg.get("subscribe",subscription);
    if (LSMessageIsSubscription (message))
    {
        jsonObject.put("subscribed", true);
        if (!LSSubscriptionProcess(sh, message, &subscription, &lserror))
        {
            lserror.Print(__FUNCTION__,__LINE__);
        }
    }
    else
    {
        jsonObject.put("subscribed", false);
    }
    jsonObject.put("returnValue", true);
    jsonObject.put("audioFocusStatus", getStatusPayload(sessionId));

    if(!LSMessageReply(sh, message, jsonObject.stringify().c_str(), &lserror))
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"sendSignal:LSMessageReply Failed");
        return false;
    }
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"getStatus: Succesfully sent response");
    return true;
}

/*Functionality of this method
 * TO get the JSON payload for getStatus response in string format
 */
pbnjson::JValue AudioFocusManager::getStatusPayload(const int& sessionId)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"getStatusPayload");

    pbnjson::JValue audioFocusStatus = pbnjson::JObject();
    pbnjson::JArray sessionsList = pbnjson::JArray();
    for (auto sessionInfomap : mSessionInfoMap)
    {
        if (sessionId == sessionInfomap.first)
        {
            SESSION_INFO_T& sessionInfo = sessionInfomap.second;
            int sessionId = sessionInfomap.first;
            pbnjson::JArray activeAppArray = pbnjson::JArray();
            pbnjson::JArray pausedAppArray = pbnjson::JArray();
            pbnjson::JValue curSession = pbnjson::JObject();

            curSession.put("sessionId", sessionId);
            for (auto activeAppInfo : sessionInfo.activeAppList)
            {
                pbnjson::JValue activeApp = pbnjson::JObject();
                activeApp.put("appId", activeAppInfo.appId);
                activeApp.put("requestType", activeAppInfo.requestType);
                activeAppArray.append(activeApp);
            }
            curSession.put("AFactiveRequests",activeAppArray);

            for (auto pausedAppInfo : sessionInfo.pausedAppList)
            {
                pbnjson::JValue pausedApp = pbnjson::JObject();
                pausedApp.put("appId", pausedAppInfo.appId);
                pausedApp.put("requestType", pausedAppInfo.requestType);
                pausedAppArray.append(pausedApp);
            }
            curSession.put("pausedRequests", pausedAppArray);
            sessionsList.append(curSession);
        }
    }

    audioFocusStatus.put("audioFocusStatus", sessionsList);
    return audioFocusStatus;
}

/*
 * Functionality of this method:
 * Notigy /getStatus subscribers on the current AudioFocusManager status
 */
void AudioFocusManager::broadcastStatusToSubscribers(int sessionId)
{
    CLSError lserror;
    std::string reply = getStatusPayload(sessionId).stringify();
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"broadcastStatusToSubscribers: reply message to subscriber: %s", \
            reply.c_str());

    if (!LSSubscriptionReply(GetLSService(), AF_API_GET_STATUS, reply.c_str(), &lserror))
    {
        lserror.Print(__FUNCTION__, __LINE__);
    }
}

/*
Functionality of this method:
->This is a utility function used for dealing with subscription list.
->Based on the operation received it does the following functionalities:-
    operation 's' : Signals the corresponding application with the signalMessage(AF_LOST/AF_PAUSE/AF_GRANTED) passed to it.
                    It will send events like AF_LOST/AF_PAUSE to the current running application.
                    It will send resume(AF_GRANTED) event to the paused application.
    operation 'r' : Removes the subscription for the respective applicationId passed.
    operation 'c' : This is for checking whether the corresponding applicationId is subscribed or not.
*/
void AudioFocusManager::manageAppSubscription(const std::string& applicationId, const std::string& payload, const char operation)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"notifyApplication: applicationId:%s payload:%s operation:%c",\
        applicationId.c_str(), payload.c_str(), operation);
    std::string subscribed_appId;
    pbnjson::JDomParser parser;
    LSSubscriptionIter *iter = NULL;
    bool retValAcquire = false;
    CLSError lserror;
    retValAcquire = LSSubscriptionAcquire(GetLSService(), "AFSubscriptionList", &iter, &lserror);
    if(retValAcquire)
    {
        while(LSSubscriptionHasNext(iter))
        {
            LSMessage *iter_message = LSSubscriptionNext(iter);
            const char* msgJsonArgument = LSMessageGetPayload(iter_message);
            parser.parse(msgJsonArgument, pbnjson::JSchemaFragment("{}"));
            pbnjson::JValue msgJsonSubArgument = parser.getDom();
            const char* appId = LSMessageGetApplicationID(iter_message);
            if (appId == NULL)
            {
                appId = LSMessageGetSenderServiceName(iter_message);
            }
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"manageAppSubscription App id :%s", appId);
            if(!((applicationId).compare(appId)))
            {
                switch(operation)
                {
                    case 's':
                            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"manageAppSubscription send response:%s",\
                                appId);
                            sendApplicationResponse(GetLSService(), iter_message, payload);
                            break;

                    case 'n':
                            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"manageAppSubscription:send response and remove %s",\
                                appId);
                            sendApplicationResponse(GetLSService(), iter_message, payload);
                            LSSubscriptionRemove(iter);
                            break;

                    case 'r':
                            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"manageAppSubscription:remove %s",\
                                appId);
                            LSSubscriptionRemove(iter);
                            break;
                    case 'c':
                            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"manageAppSubscription: Check %s",\
                                appId);
                            break;

                    default:
                            PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"manageAppSubscription: INVALID OPTION");
                            break;
                }

                lserror.Print(__FUNCTION__,__LINE__);
                break;
            }
        }
        LSSubscriptionRelease(iter);
    }
    else
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"manageAppSubscription: failed in LSSubscriptionAcquire");
}

void AudioFocusManager::sendApplicationResponse(LSHandle *serviceHandle, LSMessage *message, const std::string& payload)
{
    pbnjson::JValue replyObject = pbnjson::Object();
    replyObject.put("returnValue", true);
    replyObject.put("result", payload.c_str());
    std::string reply = replyObject.stringify();
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"sendApplicationResponse: %s", reply.c_str());
    LSMessageResponse(serviceHandle, message, reply.c_str(), eLSReply, false);
}

/*
Functionality of this method:
->This function is called when signal handlers are caught.
*/
bool AudioFocusManager::signalTermCaught()
{
    //TODO
    //broadcastLostToAll(GetLSService());
    return true;
}
