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
#if defined(WEBOS_SOC_AUTO)
    bool retVal = LSRegisterServerStatusEx(GetLSService(), ACCOUNT_SERVICE, serviceStatusCallBack, this, nullptr, nullptr);
    if (!retVal)
    {
         PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "Failed to LSRegisterServerStatusEx");
    }
#endif
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
    for (auto& it : mAFRequestPolicyInfo)
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
        int displayId = -1;
#if defined(WEBOS_SOC_AUTO)
        std::string sessionInfo = LSMessageGetSessionId(message);
        displayId = getSessionDisplayId(sessionInfo);
#else
        msg.get("displayId", displayId);
#endif
        auto itDisplay = mDisplayInfoMap.find(displayId);
        if (itDisplay == mDisplayInfoMap.end())
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: New display entry for:%d Request feasible", displayId);
            return true;
        }
        DISPLAY_INFO_T& displayInfo = itDisplay->second;
        for (auto itPaused = displayInfo.pausedAppList.begin(); \
            itPaused != displayInfo.pausedAppList.end(); itPaused++)
        {
            if (appId == itPaused->appId)
            {
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "Paused app Killed, remove from list %s Request type: %s", \
                    appId, itPaused->requestType.c_str());
                displayInfo.pausedAppList.erase(itPaused--);
                broadcastStatusToSubscribers(displayId);
                return true;
            }
        }
        for (auto itActive = displayInfo.activeAppList.begin(); itActive != displayInfo.activeAppList.end(); itActive++)
        {
            if (itActive->appId == appId)
            {
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"Active app Killed: Removing appId: %s Request type: %s", \
                    appId, itActive->requestType.c_str());
                std::string requestType = itActive->requestType;
                displayInfo.activeAppList.erase(itActive--);
                pausedAppToActive(displayInfo, requestType);
                broadcastStatusToSubscribers(displayId);
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
    int displayId = -1;
    std::string requestName;
    std::string reply;
    bool subscription;
    std::string streamType;
#if defined(WEBOS_SOC_AUTO)
    LSMessageJsonParser msg(message, STRICT_SCHEMA(PROPS_3 (PROP(requestType, string),
        PROP(subscribe, boolean), PROP(streamType, string)) REQUIRED_3(requestType, subscribe, streamType)));

    if (!msg.parse(__FUNCTION__,sh))
       return true;
    std::string sessionInfo = LSMessageGetSessionId(message);
    displayId = getSessionDisplayId(sessionInfo);
#else

    LSMessageJsonParser msg(message, STRICT_SCHEMA(PROPS_4 (PROP(requestType, string), PROP(displayId, integer),
                                    PROP(subscribe, boolean), PROP(streamType, string)) REQUIRED_4(requestType, displayId, subscribe, streamType)));

    if (!msg.parse(__FUNCTION__,sh))
       return true;
    msg.get("displayId", displayId);
#endif



    msg.get("requestType", requestName);
    msg.get("subscribe", subscription);
    msg.get("streamType", streamType);

    if (!validateDisplayId(displayId))
    {
        reply = STANDARD_JSON_ERROR(AF_ERR_CODE_INVALID_DISPLAY_ID, "Invalid displayId");
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
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "requestFocus: displayId: %d requestType: %s appId: %s streamType: %s", \
        displayId, requestName.c_str(), appId, streamType.c_str());
    if (checkGrantedAlready(sh, message, appId, displayId, requestName))
        return true;
    if (!checkFeasibility(displayId, requestName))
    {
        sendApplicationResponse(sh, message, "AF_CANNOTBEGRANTED");
        return true;
    }
    sendApplicationResponse(sh, message, "AF_GRANTED");
    if (LSMessageIsSubscription(message))
        LSSubscriptionAdd(sh, "AFSubscriptionList", message, NULL);
    updateDisplayActiveAppList(displayId, appId, requestName, streamType);
    broadcastStatusToSubscribers(displayId);
    return true;
}
/*
Functionality of this method:
Validating the display Id by matching it with the display Id
*/

#if defined(WEBOS_SOC_AUTO)
bool AudioFocusManager::validateDisplayId(int displayId)
{
    if(displayId == DISPLAY_ID_0 || displayId == DISPLAY_ID_1 || displayId == DISPLAY_ID_2)
        return true;
    else
        return false;
}
#else
bool AudioFocusManager::validateDisplayId(int displayId)
{
    if(displayId == DISPLAY_ID_0 || displayId == DISPLAY_ID_1 )
        return true;
    else
        return false;
}
#endif
/*
Functionality of this method:
->Checks whether the incoming request is duplicate request or not.
->If it is a duplicate request sends AF_GRANTEDALREADY event to the app.
*/
bool AudioFocusManager::checkGrantedAlready(LSHandle *sh, LSMessage *message, std::string applicationId,\
    const int& displayId, const std::string& requestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkGrantedAlready for appId:%s displayId:%d requestType:%s",\
        applicationId.c_str(), displayId, requestType.c_str());
    auto it = mDisplayInfoMap.find(displayId);
    if (it == mDisplayInfoMap.end())
    {
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"Request from a new display %d", displayId);
        return false;
    }
    DISPLAY_INFO_T& displayInfo = it->second;
    for (auto itPaused = displayInfo.pausedAppList.begin(); \
            itPaused != displayInfo.pausedAppList.end(); itPaused++)
    {
        if (itPaused->requestType == requestType && \
                itPaused->appId == applicationId)
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkGrantedAlready: AF_GRANTEDALREADY in paused list:%s", applicationId.c_str());
            sendApplicationResponse(sh, message, "AF_GRANTEDALREADY");
            return true;
        }
    }
    for (auto itActive = displayInfo.activeAppList.begin(); \
            itActive != displayInfo.activeAppList.end(); itActive++)
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
 * To check if active request types in the requesting display has any request which will not grant the new request type*/
bool AudioFocusManager::checkFeasibility(const int& displayId, const std::string& newRequestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility for displayId:%d newRequestType:%s",\
        displayId, newRequestType.c_str());
    auto itDisplay = mDisplayInfoMap.find(displayId);
    if (itDisplay == mDisplayInfoMap.end())
    {
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: New display entry for:%d Request feasible", displayId);
        return true;
    }
    DISPLAY_INFO_T& curdisplayInfo = itDisplay->second;
    //Check feasiblity in activeAppList pair to pair
    if (checkIncomingPair(newRequestType, curdisplayInfo.activeAppList))
    {
        for (auto itActive = curdisplayInfo.activeAppList.begin(); itActive != curdisplayInfo.activeAppList.end(); itActive++)
        {
            auto itActiveRequestPolicy = mAFRequestPolicyInfo.find(itActive->requestType);
            if (itActiveRequestPolicy != mAFRequestPolicyInfo.end())
            {
                REQUEST_TYPE_POLICY_INFO_T& activeRequestPolicy = itActiveRequestPolicy->second;
                std::string policyAction = getFocusPolicyType(newRequestType, activeRequestPolicy.incomingRequestInfo);
                if ("pause" == policyAction)
                {
                    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: send AF_PAUSE to %s", \
                        itActive->appId.c_str());
                    manageAppSubscription(itActive->appId, "AF_PAUSE", 's');
                    curdisplayInfo.pausedAppList.push_back(*itActive);
                    curdisplayInfo.activeAppList.erase(itActive--);
                }
                else if("lost" == policyAction)
                {
                    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: send AF_LOST to %s", \
                        itActive->appId.c_str());
                    manageAppSubscription(itActive->appId, "AF_LOST", 'n');
                    curdisplayInfo.activeAppList.erase(itActive--);
                }
                else
                    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: App can mix and play %s", \
                        itActive->appId.c_str());
            }
            else
                PM_LOG_WARNING(MSGID_CORE, INIT_KVCOUNT, "checkFeasibility requestType:%s not found in mAFRequestPolicyInfo", itActive->requestType.c_str());
        }
    }
    else
    {
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: newRequestType cannot be granted");
        return false;
    }
    //Check feasiblity in pausedAppList pair to pair
    //Not checking for incoming list availability for incoming request type
    for (auto itPaused = curdisplayInfo.pausedAppList.begin(); itPaused != curdisplayInfo.pausedAppList.end(); itPaused++)
    {
        auto itPausedRequestPolicy = mAFRequestPolicyInfo.find(itPaused->requestType);
        if (itPausedRequestPolicy != mAFRequestPolicyInfo.end())
        {
            REQUEST_TYPE_POLICY_INFO_T& pausedRequestPolicy = itPausedRequestPolicy->second;
            std::string policyAction = getFocusPolicyType(newRequestType, pausedRequestPolicy.incomingRequestInfo);
            if ("lost" == policyAction)
            {
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: send AF_PAUSE to %s", \
                    itPaused->appId.c_str());
                manageAppSubscription(itPaused->appId, "AF_LOST", 's');
                curdisplayInfo.pausedAppList.erase(itPaused--);
            }
            else
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: App can mix and play or already paused%s", \
                    itPaused->appId.c_str());
        }
        else
            PM_LOG_WARNING(MSGID_CORE, INIT_KVCOUNT, "checkFeasibility requestType:%s not found in mAFRequestPolicyInfo", itPaused->requestType.c_str());
    }
    return true;
}

bool AudioFocusManager::checkIncomingPair(const std::string& newRequestType, const std::list<APP_INFO_T>& appList)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkIncomingPair: newRequestType:%s", newRequestType.c_str());
    bool isPairFound = false;
    for (auto itActive = appList.begin(); itActive != appList.end(); itActive++)
    {
        auto itActiveRequestPolicy = mAFRequestPolicyInfo.find(itActive->requestType);
        if (itActiveRequestPolicy != mAFRequestPolicyInfo.end())
        {
            REQUEST_TYPE_POLICY_INFO_T& activeRequestPolicy = itActiveRequestPolicy->second;
            if (activeRequestPolicy.incomingRequestInfo.isArray())
            {
                for (const pbnjson::JValue& elements : activeRequestPolicy.incomingRequestInfo.items())
                {
                    if (elements.hasKey(newRequestType))
                    {
                        isPairFound = true;
                        break;
                    }
                }
            }
            else
                PM_LOG_WARNING(MSGID_CORE, INIT_KVCOUNT,"checkIncomingPair incomingRequestInfo not an array");
        }
        else
            PM_LOG_WARNING(MSGID_CORE, INIT_KVCOUNT,"checkIncomingPair requestType not found:%s", itActive->requestType.c_str());
        if (isPairFound)
            isPairFound = false;
        else
            return false;
    }
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkIncomingPair found in the active list");
    return true;
}

std::string AudioFocusManager::getFocusPolicyType(const std::string& newRequestType, const pbnjson::JValue& incomingRequestInfo)
{
    pbnjson::JValue requestInfo = incomingRequestInfo;
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"getFocusPolicyType: newRequestType:%s incomingRequestInfo:%s", \
        newRequestType.c_str(), requestInfo.stringify().c_str());
    std::string focusType;
    if (incomingRequestInfo.isArray())
    {
        for (const pbnjson::JValue& elements : incomingRequestInfo.items())
        {
            if (elements.hasKey(newRequestType))
            {
                if (elements[newRequestType].asString(focusType) == CONV_OK)
                    return focusType;
                else
                    PM_LOG_WARNING(MSGID_CORE, INIT_KVCOUNT,"getFocusPolicyType: focusType could not be extracted");
            }
        }
    }
    else
        PM_LOG_WARNING(MSGID_CORE, INIT_KVCOUNT,"getFocusPolicyType: incomingRequestInfo is not an array");
    return focusType;
}

/*Functionality of this methos:
 * -> Update the display active app list if display already present
 *  ->Create new display Info and update active app list
 */
void AudioFocusManager::updateDisplayActiveAppList(const int& displayId, const std::string& appId, const std::string& requestType, const std::string& streamType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateDisplayActiveAppList: displayId: %d", displayId);
    APP_INFO_T newAppInfo;
    newAppInfo.appId = appId;
    newAppInfo.requestType = requestType;
    newAppInfo.streamType = streamType;
    auto itDisplay = mDisplayInfoMap.find(displayId);
    if (itDisplay == mDisplayInfoMap.end())
    {
        DISPLAY_INFO_T newdisplayInfo;
        newdisplayInfo.activeAppList.push_back(newAppInfo);
        mDisplayInfoMap[displayId] = newdisplayInfo;
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateDisplayActiveAppList: new display details added. Display: %d", \
            displayId);
        return;
    }
    DISPLAY_INFO_T& displayInfo = itDisplay->second;
    displayInfo.activeAppList.push_back(newAppInfo);
}

/*
Functionality of this method:
->This will unsubscribe the app and removes its entry.
->Sends resume event to the application which is in paused state if there are any such applications.
*/
bool AudioFocusManager::releaseFocus(LSHandle *sh, LSMessage *message, void *data)
{
    int displayId = -1;
    std::string reply;
    std::string streamType;
#if defined(WEBOS_SOC_AUTO)
    LSMessageJsonParser msg(message, STRICT_SCHEMA(PROPS_1(PROP(streamType, string)) REQUIRED_1(streamType)));
    if (!msg.parse(__FUNCTION__, sh))
        return true;
    std::string sessionInfo = LSMessageGetSessionId(message);
    displayId = getSessionDisplayId(sessionInfo);
#else
    LSMessageJsonParser msg(message, STRICT_SCHEMA(PROPS_2(PROP(displayId, integer), PROP(streamType, string)) REQUIRED_2(displayId, streamType)));
    if (!msg.parse(__FUNCTION__, sh))
        return true;
    msg.get("displayId", displayId);
#endif

    msg.get("streamType", streamType);

    if (!validateDisplayId(displayId))
    {
        reply = STANDARD_JSON_ERROR(AF_ERR_CODE_INVALID_DISPLAY_ID, "Invalid displayId");
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
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"releaseFocus: displayId: %d appId: %s streamType: %s", displayId, appId, streamType.c_str());
    auto itDisplay = mDisplayInfoMap.find(displayId);
    if (itDisplay == mDisplayInfoMap.end())
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"releaseFocus: display ID cannot be find");
        reply = STANDARD_JSON_ERROR(AF_ERR_CODE_INTERNAL, "No active requests found for the application");
        LSMessageResponse(sh, message, reply.c_str(), eLSReply, false);
        return true;
    }
    DISPLAY_INFO_T& curdisplayInfo = itDisplay->second;
    for (auto itPaused = curdisplayInfo.pausedAppList.begin(); itPaused != curdisplayInfo.pausedAppList.end(); itPaused++)
    {
        if (itPaused->appId == appId)
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "releaseFocus: Removing appId: %s Request type: %s", \
                appId, itPaused->requestType.c_str());
            manageAppSubscription(appId, "AF_RELEASED", 'r');
            curdisplayInfo.pausedAppList.erase(itPaused--);
            broadcastStatusToSubscribers(displayId);
            sendApplicationResponse(sh, message, "AF_SUCCESSFULLY_RELEASED");
            return true;
        }
    }

    for (auto itActive = curdisplayInfo.activeAppList.begin(); itActive != curdisplayInfo.activeAppList.end(); itActive++)
    {
        if (itActive->appId == appId)
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"releaseFocus: Removing appId: %s Request type: %s", \
                appId, itActive->requestType.c_str());
            manageAppSubscription(appId, "AF_RELEASED", 'r');
            std::string requestType = itActive->requestType;
            curdisplayInfo.activeAppList.erase(itActive--);
            pausedAppToActive(curdisplayInfo, requestType);
            broadcastStatusToSubscribers(displayId);
            sendApplicationResponse(sh, message, "AF_SUCCESSFULLY_RELEASED");
            return true;
        }
    }

    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "releaseFocus: appId: %s, streamType: %s is not found in display: %d" , \
        appId, streamType.c_str(), displayId);
    reply = STANDARD_JSON_ERROR(AF_ERR_CODE_INTERNAL, "Application not registered");
    LSMessageResponse(sh, message, reply.c_str(), eLSReply, false);
    return true;
}

bool AudioFocusManager::pausedAppToActive(DISPLAY_INFO_T& displayInfo, const std::string& removedRequest)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "pausedAppToActive for removedRequest:%s", removedRequest.c_str());
    if (displayInfo.pausedAppList.size() == 1 && displayInfo.activeAppList.empty())
    {
        auto itPaused = displayInfo.pausedAppList.back();
        displayInfo.activeAppList.push_back(itPaused);
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "pausedAppToActive: send AF_GRANETD to %s", itPaused.appId.c_str());
        manageAppSubscription(itPaused.appId, "AF_GRANTED", 's');
        displayInfo.pausedAppList.pop_back();
    }
    else
    {
        for (auto itPaused = displayInfo.pausedAppList.begin(); itPaused != displayInfo.pausedAppList.end(); itPaused++)
        {
            auto itPausedRequestPolicy = mAFRequestPolicyInfo.find(itPaused->requestType);
            if (itPausedRequestPolicy != mAFRequestPolicyInfo.end())
            {
                REQUEST_TYPE_POLICY_INFO_T& pausedRequestPolicy = itPausedRequestPolicy->second;
                std::string policyAction = getFocusPolicyType(removedRequest, pausedRequestPolicy.incomingRequestInfo);
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"pausedAppToActive policyAction:%s", policyAction.c_str());
                if ("pause" == policyAction && isIncomingPairRequestTypeActive(itPaused->requestType, displayInfo))
                {
                    displayInfo.activeAppList.push_back(*itPaused);
                    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "pausedAppToActive: send AF_GRANETD to %s", itPaused->appId.c_str());
                    manageAppSubscription(itPaused->appId, "AF_GRANTED", 's');
                    itPaused = displayInfo.pausedAppList.erase(itPaused);
                    --itPaused;
                }
                else
                    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"pausedAppToActive incomingPairRequestType is not active");
            }
            else
                PM_LOG_WARNING(MSGID_CORE, INIT_KVCOUNT,"pausedAppToActive requestType not found:%s", itPaused->requestType.c_str());
        }
    }
    return true;
}

//Check if any other active app incoming request list does not have pause for already paused app, do not resume in that case
bool AudioFocusManager::isIncomingPairRequestTypeActive(const std::string& requestType, const DISPLAY_INFO_T& displayInfo)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "isIncomingPairRequestTypeActive for requestType:%s", requestType.c_str());
    for (auto itActive = displayInfo.activeAppList.begin(); itActive != displayInfo.activeAppList.end(); itActive++)
    {
        auto itPausedRequestPolicy = mAFRequestPolicyInfo.find(requestType);
        if (itPausedRequestPolicy != mAFRequestPolicyInfo.end())
        {
            REQUEST_TYPE_POLICY_INFO_T& pausedRequestPolicy = itPausedRequestPolicy->second;
            if (pausedRequestPolicy.incomingRequestInfo.isArray())
            {
                std::string policyAction = getFocusPolicyType(itActive->requestType, pausedRequestPolicy.incomingRequestInfo);
                if (("pause" == policyAction) || policyAction.empty() || ("lost" == policyAction))
                    return false;
            }
            else
                PM_LOG_WARNING(MSGID_CORE, INIT_KVCOUNT,"isIncomingPairRequestTypeActive incomingRequestInfo not an array");
        }
        else
            PM_LOG_WARNING(MSGID_CORE, INIT_KVCOUNT,"isIncomingPairRequestTypeActive requestType not found:%s", itActive->requestType.c_str());
    }
    return true;
}

/*
Functionality of this method:
->This will return the current status of granted request types in AudioFocusManager for each display
*/
bool AudioFocusManager::getStatus(LSHandle *sh, LSMessage *message, void *data)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"getStatus");
    CLSError lserror;
    pbnjson::JValue jsonObject = pbnjson::JObject();
    std::string reply;
    bool subscription;
    int displayId = -1;
    std::string streamType;
#if defined(WEBOS_SOC_AUTO)
    LSMessageJsonParser msg(message, STRICT_SCHEMA(PROPS_1(PROP(subscribe, boolean))));

    if (!msg.parse(__FUNCTION__, sh))
        return true;
    std::string sessionInfo = LSMessageGetSessionId(message);
    displayId = getSessionDisplayId(sessionInfo);
#else
    LSMessageJsonParser msg(message, STRICT_SCHEMA(PROPS_2(PROP(subscribe, boolean), PROP(displayId, integer))
        REQUIRED_1(displayId)));
    if (!msg.parse(__FUNCTION__, sh))
        return true;
    msg.get("displayId", displayId);

#endif

    if (!validateDisplayId(displayId))
    {
        reply = STANDARD_JSON_ERROR(AF_ERR_CODE_INVALID_DISPLAY_ID, "Invalid displayId");
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
    jsonObject.put("audioFocusStatus", getStatusPayload(displayId));

    if(!LSMessageReply(sh, message, jsonObject.stringify().c_str(), &lserror))
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"sendSignal:LSMessageReply Failed");
        return false;
    }
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"getStatus: Succesfully sent response, streamType: %s displayId:%d", streamType.c_str(), displayId);
    return true;
}

/*Functionality of this method
 * TO get the JSON payload for getStatus response in string format
 */
pbnjson::JValue AudioFocusManager::getStatusPayload(const int& displayId)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"getStatusPayload");

    pbnjson::JValue audioFocusStatus = pbnjson::JObject();
    pbnjson::JValue curDisplay = pbnjson::JObject();
    pbnjson::JArray displaysList = pbnjson::JArray();
    pbnjson::JArray activeAppArray = pbnjson::JArray();
    pbnjson::JArray pausedAppArray = pbnjson::JArray();
    for (auto displayInfomap : mDisplayInfoMap)
    {
        if (displayId == displayInfomap.first)
        {
            DISPLAY_INFO_T& displayInfo = displayInfomap.second;
            for (auto activeAppInfo : displayInfo.activeAppList)
            {
                pbnjson::JValue activeApp = pbnjson::JObject();
                activeApp.put("appId", activeAppInfo.appId);
                activeApp.put("requestType", activeAppInfo.requestType);
                activeApp.put("streamType", activeAppInfo.streamType);
                activeAppArray.append(activeApp);
            }
            for (auto pausedAppInfo : displayInfo.pausedAppList)
            {
                pbnjson::JValue pausedApp = pbnjson::JObject();
                pausedApp.put("appId", pausedAppInfo.appId);
                pausedApp.put("requestType", pausedAppInfo.requestType);
                pausedApp.put("streamType", pausedAppInfo.streamType);
                pausedAppArray.append(pausedApp);
            }
        }
    }
    curDisplay.put("displayId", displayId);
    curDisplay.put("pausedRequests", pausedAppArray);
    curDisplay.put("activeRequests",activeAppArray);
    displaysList.append(curDisplay);

    return displaysList;
}

/*
 * Functionality of this method:
 * Notigy /getStatus subscribers on the current AudioFocusManager status
 */
void AudioFocusManager::broadcastStatusToSubscribers(int displayId)
{
    CLSError lserror;
    pbnjson::JValue jsonObject = pbnjson::JObject();
    jsonObject.put("returnValue",true);
    jsonObject.put("subscribed",true);
    jsonObject.put("audioFocusStatus", getStatusPayload(displayId));
    std::string reply = jsonObject.stringify();
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
            if (!parser.parse(msgJsonArgument, pbnjson::JSchemaFragment("{}")))
            {
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"Not a valid json payload");
                continue;
            }
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
    if (payload != "AF_SUCCESSFULLY_RELEASED")
        replyObject.put("subscribed", true);
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
