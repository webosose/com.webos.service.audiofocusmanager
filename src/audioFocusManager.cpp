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
        return AFService;
    }
    else
    {
        return AFService;
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
        std::string type;
        if (elements["request"].asString(requestType) == CONV_OK)
        {
            stPolicyInfo.priority = elements["priority"].asNumber<int>();
            if (elements["type"].asString(type) == CONV_OK)
                stPolicyInfo.type = type;
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
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "RequestType: %s  Priority: %d Type: %s", it.first.c_str(), \
            it.second.priority, it.second.type.c_str());
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

    LSMessageJsonParser msg(message, STRICT_SCHEMA(PROPS_3 (PROP(requestType, string), PROP(sessionId, string),
                                    PROP(subscribe, boolean)) REQUIRED_3(requestType, sessionId,subscribe)));

    if (!msg.parse(__FUNCTION__,sh))
       return true;

    std::string sessionId ;
    std::string requestName;
    bool subscription;

    msg.get("sessionId", sessionId);
    msg.get("requestType", requestName);
    msg.get("subscribe", subscription);

    //TODO: Add validation logic for validation of session ID
    if (sessionId == "")
    {
        if (!returnErrorText(sh, message, "sessionId cannot be empty", AF_ERR_CODE_INTERNAL))
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT,\
                "requestFocus:Failed to sent error text for Internal Error due to empty sessionId");
        return true;
    }

    const char* appId = LSMessageGetApplicationID(message);
    if (appId == NULL)
    {
        appId = LSMessageGetSenderServiceName(message);
        if (appId == NULL)
        {
            if (!returnErrorText(sh, message, "appId received as NULL", AF_ERR_CODE_INTERNAL))
                PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT,\
                    "requestFocus: Failed to sent error text for Internal Error due to NULL appId");
            return true;
        }
    }

    const auto& it = mAFRequestPolicyInfo.find(requestName);
    if (it != mAFRequestPolicyInfo.end())
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "Valid request type received");
    else
    {
        if (!returnErrorText(sh, message, "Invalid Request Type", AF_ERR_CODE_UNKNOWN_REQUEST))
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT,\
                "requestFocus: Failed to sent error text for Invalid Request Type to %s", appId);
        return true;
    }

    if (!subscription)
    {
        if (!returnErrorText(sh, message, "Subscription should be true", AF_ERR_CODE_INTERNAL))
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT,\
                "requestFocus: Failed to sent error text for Internal Error due to incorrect subscription value to %s", appId);
        return true;
    }

    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "requestFocus: sessionId: %s requestType: %s appId %s", \
        sessionId.c_str(), requestName.c_str(), appId);

    //Check if request type is already granted for the resource
    if (checkGrantedAlready(sh, message, appId, sessionId, requestName))
        return true;
    if (!checkFeasibility(sessionId, requestName))
    {
        //TO DO add LSMessageResponse/Reply util function
        //Reply CANNOT_BE_GRANTED
        if (!sendSignal(sh, message, "AF_CANNOTBEGRANTED", NULL))
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestFocus: sendSignal failed : AF_CANNOTBEGRANTED");
        return true;
    }
    if (!updateCurrentAppStatus(sessionId, requestName))
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestFocus: updateCurrentAppStatus Failed");
    //TO DO add LSMessageResponse/Reply util function
    if (!sendSignal(sh, message, "AF_GRANTED", NULL))
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestFocus: sendSignal failed : AF_GRANTED");
        //TODO: Add failover steps, update paused list if needed
        return true;
    }
    if (LSMessageIsSubscription(message))
        LSSubscriptionAdd(sh, "AFSubscriptionlist", message, NULL);
    updateSessionActiveAppList(sessionId, appId, requestName);
    broadcastStatusToSubscribers();
    return true;
}

/*
Functionality of this method:
->Checks whether the incoming request is duplicate request or not.
->If it is a duplicate request sends AF_GRANTEDALREADY event to the app.
*/
bool AudioFocusManager::checkGrantedAlready(LSHandle *sh, LSMessage *message, std::string applicationId,\
    const std::string& sessionId, const std::string& requestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkGrantedAlready for appId:%s sessionId:%s requestType:%s",\
        applicationId.c_str(), sessionId.c_str(), requestType.c_str());
    auto it = mSessionInfoMap.find(sessionId);
    if (it == mSessionInfoMap.end())
    {
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"Request from a new session %s", sessionId.c_str());
        return false;
    }
    SESSION_INFO_T& sessionInfo = it->second;
    for (auto itPaused = sessionInfo.pausedAppList.begin(); \
            itPaused != sessionInfo.pausedAppList.end(); itPaused++)
    {
        if (itPaused->requestType == requestType && \
                itPaused->appId == applicationId)
        {
            if(!sendSignal(sh, message, "AF_GRANTEDALREADY", NULL))
            {
                //unsubscribe the app
                if (!unsubscribingApp(applicationId))
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "checkGrantedAlready: unsubscribing failed for %s", applicationId.c_str());
                sessionInfo.pausedAppList.erase(itPaused--);
                PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "checkGrantedAlready: Failed to send AF_GRANTEDALREADY %s", applicationId.c_str());
                return true;

            }
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkGrantedAlready: AF_GRANTEDALREADY %s", applicationId.c_str());
            return true;
        }
    }

    for (auto itActive = sessionInfo.activeAppList.begin(); \
            itActive != sessionInfo.activeAppList.end(); itActive++)
    {
        if (itActive->requestType == requestType && \
                itActive->appId == applicationId)
        {
            if(!sendSignal(sh, message, "AF_GRANTEDALREADY", NULL))
            {
                //unsubscribe the app
                if (!unsubscribingApp(applicationId))
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "checkGrantedAlready: unsubscribing failed for %s", applicationId.c_str());
                sessionInfo.activeAppList.erase(itActive--);
                PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "checkGrantedAlready: Failed to send AF_GRANTEDALREADY %s", applicationId.c_str());
                return true;

            }
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkGrantedAlready: AF_GRANTEDALREADY");
            return true;
        }
    }

    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "checkGrantedAlready: Request shall be approved for %s", applicationId.c_str());
    return false;
}

/*Functionality of this method:
 * To check if active request types in the requesting session has any request which will not grant the new request type*/
bool AudioFocusManager::checkFeasibility(const std::string& sessionId, const std::string& newRequestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility for sessionId:%s newRequestType:%s", sessionId.c_str(), newRequestType.c_str());
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
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: New session entry for:%s Request feasible", sessionId.c_str());
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
        if (activeRequestPolicy.priority < newRequestPolicy.priority && \
                activeRequestPolicy.type != "mix")
        {
            //Check if the app is alive
            if (!checkSubscription(itActive.appId))
            {
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: active app : %s is not alive", itActive.appId.c_str());
                //TODO: Remove from the list and see if pausedapp list needs any update
                continue;
            }
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: newrequestType: %s cannot be granted due to active request: %s", \
                newRequestType.c_str(), activeRequest.c_str());
            return false;
        }
    }
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: New requestType: %s shall be approved", \
        newRequestType.c_str());
    return true;
}

bool AudioFocusManager::updateCurrentAppStatus(const std::string& sessionId, const std::string& newRequestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus for sessionId:%s requestType:%s",\
        sessionId.c_str(), newRequestType.c_str());
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
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "updateCurrentAppStatus: new session requested: %s No need to update app status", \
            sessionId.c_str());
        return true;
    }
    SESSION_INFO_T& curSessionInfo = itSession->second;
    //Update active app list based on new request
    for (auto itActive = curSessionInfo.activeAppList.begin(); itActive != curSessionInfo.activeAppList.end(); itActive++)
    {
        const auto& itActiveRequestPolicy = mAFRequestPolicyInfo.find(itActive->requestType);
        if (itActiveRequestPolicy == mAFRequestPolicyInfo.end())
        {
            PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Invalid requestType in active list for %s. Skipping", \
                    sessionId.c_str());
            continue;
        }
        REQUEST_TYPE_POLICY_INFO_T& activeRequestPolicy = itActiveRequestPolicy->second;
        if (activeRequestPolicy.priority >= newRequestPolicy.priority)
        {
            if (newRequestPolicy.type == "short" && \
                   (activeRequestPolicy.type == "long" || activeRequestPolicy.type == "mix"))
            {
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: send AF_PAUSE to %s", \
                    itActive->appId.c_str());
                if (!signalToApp(itActive->appId, "AF_PAUSE"))
                {
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to sent AF_PAUSE to %s", \
                        itActive->appId.c_str());
                    if (!unsubscribingApp(itActive->appId))
                        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to Unsubscribe app: %s", \
                            itActive->appId.c_str());
                    curSessionInfo.activeAppList.erase(itActive--);
                    continue;
                }
                curSessionInfo.pausedAppList.push_back(*itActive);
                curSessionInfo.activeAppList.erase(itActive--);
            }
            else if (newRequestPolicy.type != "mix")
            {
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: send AF_LOST to %s", \
                        itActive->appId.c_str());
                if (!signalToApp(itActive->appId, "AF_LOST"))
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to sent AF_LOST to %s", \
                        itActive->appId.c_str());
                if (!unsubscribingApp(itActive->appId))
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to Unsubscribe to active app: %s", \
                        itActive->appId.c_str());
                curSessionInfo.activeAppList.erase(itActive--);
            }
            else
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: App can mix and play %s", \
                    itActive->appId.c_str());
        }
    }
    //Update paused apps list based on new request
    for (auto itPaused = curSessionInfo.pausedAppList.begin(); itPaused != curSessionInfo.pausedAppList.end(); itPaused++)
    {
        const auto& itPausedRequestPolicy = mAFRequestPolicyInfo.find(itPaused->requestType);
        if (itPausedRequestPolicy == mAFRequestPolicyInfo.end())
        {
            PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Invalid requestType in paused list for %s. Skipping", \
                    sessionId.c_str());
            continue;
        }
        if (newRequestPolicy.type == "long")
        {
            if (!signalToApp(itPaused->appId, "AF_LOST"))
            {
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to sent AF_LOST to %s", \
                        itPaused->appId.c_str());
                    if (!unsubscribingApp(itPaused->appId))
                        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to Unsubscribe to paused app");
            }
            if (!unsubscribingApp(itPaused->appId))
                PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: unsubscribing failed");
            curSessionInfo.pausedAppList.erase(itPaused--);
        }
    }
    return true;
}

/*Functionality of this methos:
 * -> Update the session active app list if session already present
 *  ->Create new session Info and update active app list
 */
void AudioFocusManager::updateSessionActiveAppList(const std::string& sessionId, const std::string& appId, const std::string& requestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateSessionAppList: sessionId: %s", sessionId.c_str());
    APP_INFO_T newAppInfo;
    newAppInfo.appId = appId;
    newAppInfo.requestType = requestType;
    auto itSession = mSessionInfoMap.find(sessionId);
    if (itSession == mSessionInfoMap.end())
    {
        SESSION_INFO_T newSessionInfo;
        newSessionInfo.activeAppList.push_back(newAppInfo);
        mSessionInfoMap[sessionId] = newSessionInfo;
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateSessionActiveAppList: new session details added. Session: %s", \
            sessionId.c_str());
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
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"releaseFocus");

    LSMessageJsonParser msg(message, STRICT_SCHEMA(PROPS_1(PROP(sessionId,string)) REQUIRED_1(sessionId)));

    if (!msg.parse(__FUNCTION__, sh))
        return true;

    std::string sessionId;
    msg.get("sessionId", sessionId);

    const char* appId = LSMessageGetApplicationID(message);
    if (appId == NULL)
    {
        appId = LSMessageGetSenderServiceName(message);
        if (appId == NULL)
        {
            if (!returnErrorText(sh, message, "Internal error", AF_ERR_CODE_INTERNAL))
                PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT,\
                    "releaseFocus: Failed to sent error text for Internal Error due to NULL appId to %s", appId);
            return true;
        }
    }
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"releaseFocus: sessionId: %s appId: %s", sessionId.c_str(), appId);

    auto itSession = mSessionInfoMap.find(sessionId);
    if (itSession == mSessionInfoMap.end())
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"releaseFocus: Session ID cannot be find");
        if (!returnErrorText(sh, message, "Session Id not found", AF_ERR_CODE_INTERNAL))
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT,\
                "releaseFocus: Failed to sent error text for Internal Error due to session ID not found");
        return true;
    }
    SESSION_INFO_T& curSessionInfo = itSession->second;
    for (auto itPaused = curSessionInfo.pausedAppList.begin(); itPaused != curSessionInfo.pausedAppList.end(); itPaused++)
    {
        if (itPaused->appId == appId)
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "releaseFocus: Removing appId: %s Request type: %s", \
                appId, itPaused->requestType.c_str());
            if (!unsubscribingApp(appId))
                PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "releaseFocus: unsubscribingApp failed for %s", appId);
            curSessionInfo.pausedAppList.erase(itPaused--);
            if (!sendSignal(sh, message, "AF_SUCCESSFULLY_RELEASED", NULL))
                PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "releaseFocus: Failed to sent message: AF_SUCCESSFULLY_RELEASED");
            return true;
        }
    }

    for (auto itActive = curSessionInfo.activeAppList.begin(); itActive != curSessionInfo.activeAppList.end(); itActive++)
    {
        if (itActive->appId == appId)
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"releaseFocus: Removing appId: %s Request type: %s", \
                appId, itActive->requestType.c_str());
            if (!unsubscribingApp(appId))
                PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "releaseFocus: unsubscribingApp failed for %s", appId);
            std::string requestType = itActive->requestType;
            curSessionInfo.activeAppList.erase(itActive--);
            updatePausedAppStatus(curSessionInfo, requestType);
            if (!sendSignal(sh, message, "AF_SUCCESSFULLY_RELEASED", NULL))
                PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "releaseFocus: Failed to sent message: AF_SUCCESSFULLY_RELEASED");
            return true;
        }
    }

    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "releaseFocus: appId: %s cannot be found in session: %s" , \
        appId, sessionId.c_str());
    if (!returnErrorText(sh, message, "Application not registered", AF_ERR_CODE_INTERNAL))
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT,\
            "releaseFocus: Failed to sent error text for Internal Error:application not registered");
    return true;
}

/*Functionality of this method:
 * To update the paused app list in the session based on the removed request type policy
 */
void AudioFocusManager::updatePausedAppStatus(SESSION_INFO_T& sessionInfo, const std::string& removedRequest)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "updatePausedAppStatus requestType:%s", removedRequest.c_str());
    const auto& requestInfoIt = mAFRequestPolicyInfo.find(removedRequest);
    if(requestInfoIt == mAFRequestPolicyInfo.end())
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "updatePausedAppStatus: unknown request type");
        return;
    }
    if (requestInfoIt->second.type != "short")
    {
        PM_LOG_INFO(MSGID_INIT, INIT_KVCOUNT, "updatePausedAppStatus: request type is not short. Nothing to update");
        return;
    }
    for( auto activeApp : sessionInfo.activeAppList)
    {
        auto activeRequestInfo = mAFRequestPolicyInfo.find(activeApp.requestType);
        if (activeRequestInfo == mAFRequestPolicyInfo.end())
        {
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "updatePausedAppStatus: unknown request type in active app");
            continue;
        }
        if (activeRequestInfo->second.type == "short")
        {
            PM_LOG_INFO(MSGID_INIT, INIT_KVCOUNT, "updatePausedAppStatus: other short streams active. Nothing to update");
            return;
        }
    }
    for (auto itPaused = sessionInfo.pausedAppList.begin(); itPaused != sessionInfo.pausedAppList.end(); itPaused++)
    {
        sessionInfo.pausedAppList.push_back(*itPaused);
        sessionInfo.pausedAppList.erase(itPaused--);
    }
    return;
}
/*
Functionality of this method:
->This will return the current status of granted request types in AudioFocusManager for each session
*/
bool AudioFocusManager::getStatus(LSHandle *sh, LSMessage *message, void *data)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"getStatus");
    CLSError lserror;

    LSMessageJsonParser msg(message, STRICT_SCHEMA(PROPS_1(PROP(subscribe, string))));

    if (!msg.parse(__FUNCTION__, sh))
        return true;
    bool subscription;

    msg.get("subscribe",subscription);
    if (LSMessageIsSubscription (message))
    {
        if (!LSSubscriptionProcess(sh, message, &subscription, &lserror))
        {
            lserror.Print(__FUNCTION__,__LINE__);
        }
    }

    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"%s : %d", __FUNCTION__, __LINE__);
    pbnjson::JValue jsonObject = pbnjson::JObject();

    jsonObject.put("returnValue", true);
    jsonObject.put("audioFocusStatus", getStatusPayload());

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
pbnjson::JValue AudioFocusManager::getStatusPayload()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"getStatusPayload");

    pbnjson::JValue audioFocusStatus = pbnjson::JObject();
    pbnjson::JArray sessionsList = pbnjson::JArray();
    for (auto sessionInfomap : mSessionInfoMap)
    {
        SESSION_INFO_T& sessionInfo = sessionInfomap.second;
        std::string sessionId = sessionInfomap.first;
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

    audioFocusStatus.put("audioFocusStatus", sessionsList);
    return audioFocusStatus;
}

/*
 * Functionality of this method:
 * Notigy /getStatus subscribers on the current AudioFocusManager status
 */
void AudioFocusManager::broadcastStatusToSubscribers()
{
    LSError lserror;
    LSErrorInit(&lserror);
    std::string reply = getStatusPayload().stringify();
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"broadcastStatusToSubscribers: reply message to subscriber: %s", \
            reply.c_str());
    //TODO: This is deprecated method for accessing LsHandle.
    //Once the entire LS subscription mechanism is updated for the new API sets this also will be upgraded.

    if (!LSSubscriptionReply(GetLSService(), AF_API_GET_STATUS, reply.c_str(), &lserror))
    {
        LSErrorPrintAndFree(&lserror);
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
bool AudioFocusManager::subscriptionUtility(const std::string& applicationId, LSHandle *serviceHandle, const char operation, const std::string& signalMessage)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"subscriptionUtility: Operation: %c, appId: %s", operation, applicationId.c_str());
    std::string subscribed_appId;
    pbnjson::JDomParser parser(NULL);
    LSSubscriptionIter *iter = NULL;
    bool retValAcquire = false,found = false;
    LSError lserror;
    LSErrorInit(&lserror);
    retValAcquire = LSSubscriptionAcquire(serviceHandle, "AFSubscriptionlist", &iter, &lserror);

    if(retValAcquire)
    {
        while(LSSubscriptionHasNext(iter))
        {
            LSMessage *iter_message = LSSubscriptionNext(iter);
            const char* msgJsonArgument = LSMessageGetPayload(iter_message);
            parser.parse(msgJsonArgument, pbnjson::JSchemaFragment("{}"), NULL);
            pbnjson::JValue msgJsonSubArgument = parser.getDom();
            subscribed_appId = msgJsonSubArgument["appId"].asString();

            if(!((applicationId).compare(subscribed_appId)))
            {
                found = true;

                switch(operation)
                {
                    case 's':
                            if(!sendSignal(serviceHandle, iter_message, signalMessage, &lserror))
                            {
                                found = false;
                            }
                            break;

                    case 'r':
                            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"subscriptionUtility: Remove %s", subscribed_appId.c_str());
                            LSSubscriptionRemove(iter);
                            break;

                    case 'c':
                            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"subscriptionUtility: Check %s", subscribed_appId.c_str());
                            break;

                    default:
                            PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"subscriptionUtility: INVALID OPTION");
                            break;
                }

                LSErrorPrint(&lserror, stderr);
                LSErrorFree(&lserror);
                break;
            }
        }

        LSSubscriptionRelease(iter);

        if(found)
        {
            return true;
        }

        return false;
    }
    else
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"subscriptionUtility: failed in LSSubscriptionAcquire");
        return false;
    }
}

bool AudioFocusManager::sendSignal(LSHandle *serviceHandle, LSMessage *iter_message, const std::string& signalMessage, LSError *lserror)
{
    pbnjson::JValue jsonObject = pbnjson::Object();
    //jsonObject.put("errorCode", 0);
    jsonObject.put("returnValue", true);
    jsonObject.put("result", signalMessage.c_str());
    if(!LSMessageReply(serviceHandle, iter_message, pbnjson::JGenerator::serialize(jsonObject, pbnjson::JSchemaFragment("{}")).c_str(), lserror))
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"sendSignal:LSMessageReply Failed");
        return false;
    }

    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"sendSignal: %s", signalMessage.c_str());
    return true;
}

/*
Functionality of this method:
->It will send error message to the applications.
*/
bool AudioFocusManager::returnErrorText(LSHandle *sh, LSMessage *message, const std::string& errorText, int errorCode)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"returnErrorText: %s", errorText.c_str());

    pbnjson::JValue jsonObject = pbnjson::Object();
    jsonObject.put("errorCode", errorCode);
    jsonObject.put("returnValue", false);
    jsonObject.put("errorText", errorText.c_str());

    std::string reply = pbnjson::JGenerator::serialize(jsonObject, pbnjson::JSchemaFragment("{}"));

    if(!LSMessageReply(sh, message, reply.c_str(), NULL))
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"returning error text: %s failed", errorText.c_str());
        return false;
    }

    return true;
}

/*
Functionality of this method:
->This will Unsubscribe the application.
*/
bool AudioFocusManager::unsubscribingApp(const std::string& applicationId)
{
    //TODO:Implement LS subscription
    return true;
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"unsubscribingApp: %s", applicationId.c_str());

    if(subscriptionUtility(applicationId, GetLSService(), 'r', ""))
    {
        return true;
    }

    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"unsubscribingApp: failed in LSSubscriptionAcquire");
    return false;
}

/*
Functionality of this method:
->This will check whether the application is alive or not.
*/
bool AudioFocusManager::checkSubscription(const std::string& applicationId)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkSubscription: %s", applicationId.c_str());

    if(subscriptionUtility(applicationId, GetLSService(), 'c', ""))
    {
        return true;
    }

    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"checkSubscription: failed in LSSubscriptionAcquire %s", applicationId.c_str());
    return false;
}

/*
Functionality of this method:
->Prints the error message.
*/
void AudioFocusManager::LSErrorPrintAndFree(LSError *ptrLSError)
{
    if(ptrLSError != NULL)
    {
        LSErrorPrint(ptrLSError, stderr);
        LSErrorFree(ptrLSError);
    }
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

bool AudioFocusManager::signalToApp(const std::string& appId, const std::string& returnType)
{
    return true;
}