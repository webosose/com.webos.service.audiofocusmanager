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


#include <sys/time.h>
#include <pbnjson.hpp>
#include <glib.h>
#include <lunaservice.h>
#include <map>
#include <sstream>
#include "log.h"
#include <audioFocusManager.h>

const std::string requestAudioControlschema      =  "{\
                                     \"title\":\"requestAudioControl schema\",\
                                     \"type\" :\"object\",\
                                     \"properties\": {\
                                         \"requestType\": {\"type\":\"string\"},\
                                         \"sessionId\": {\"type\":\"string\"},\
                                         \"subscribe\": {\"type\":\"boolean\"}\
                                       },\
                                     \"required\" :[\"requestType\", \"sessionId\",\"subscribe\"]\
                                    }";

const std::string releaseAudioControlschema      =  "{\
                                     \"title\":\"releaseAudioControl schema\",\
                                     \"type\" :\"object\",\
                                     \"properties\": {\
                                         \"sessionId\": {\"type\":\"string\"}\
                                       },\
                                     \"required\" :[\"sessionId\"]\
                                    }";

const std::string getStatusschema                =  "{\
                                     \"title\":\"getStatus schema\",\
                                     \"type\" :\"object\",\
                                     \"properties\": {\
                                         \"subscribe\": {\"type\":\"boolean\"}\
                                       }\
                                    }";

LSHandle *AudioFocusManager::mServiceHandle;

LSMethod AudioFocusManager::rootMethod[] = {
    {"requestAudioControl", AudioFocusManager::_requestAudioControl},
    {"releaseAudioControl", AudioFocusManager::_releaseAudioControl},
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
AudioFocusManager *AudioFocusManager::getInstance()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "getInstance");
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

    if(audioServiceRegister("com.webos.service.audiofocusmanager", mainLoop, &mServiceHandle) == false)
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "com.webos.service.audiofocusmanager service registration failed");
        return false;
    }

    createMapRequestNametoType();
    createMapRequestTypetoName();

    if(loadRequestPolicyJsonConfig() == false)
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "Failed to parse RequestPolicy Json config");
        return false;
    }

    printRequestPolicyJsonInfo();
    return true;
}

void AudioFocusManager::createMapRequestNametoType()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "createMapRequestNametoType");

    mRequestNameToType["AFREQUEST_GAIN"] = common::RequestType::GAIN;
    mRequestNameToType["AFREQUEST_MUTE_LONG"] = common::RequestType::MUTE_LONG;
    mRequestNameToType["AFREQUEST_MUTE_SHORT"] = common::RequestType::MUTE_SHORT;
    mRequestNameToType["AFREQUEST_CALL"] = common::RequestType::CALL;
    mRequestNameToType["AFREQUEST_VOICE"] = common::RequestType::MUTE_VOICE;
    mRequestNameToType["AFREQUEST_MUTE_MUTUAL"] = common::RequestType::MUTE_MUTUAL;
    mRequestNameToType["AFREQUEST_TRANSIENT"] = common::RequestType::TRANSIENT;
}

void AudioFocusManager::createMapRequestTypetoName()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "createMapRequestTypetoName");

    mRequestTypeToName[common::RequestType::GAIN] = "AFREQUEST_GAIN";
    mRequestTypeToName[common::RequestType::MUTE_LONG] = "AFREQUEST_MUTE_LONG";
    mRequestTypeToName[common::RequestType::MUTE_SHORT] = "AFREQUEST_MUTE_SHORT";
    mRequestTypeToName[common::RequestType::CALL] = "AFREQUEST_CALL";
    mRequestTypeToName[common::RequestType::MUTE_VOICE] = "AFREQUEST_VOICE";
    mRequestTypeToName[common::RequestType::MUTE_MUTUAL] = "AFREQUEST_MUTE_MUTUAL";
    mRequestTypeToName[common::RequestType::TRANSIENT] = "AFREQUEST_TRANSIENT";
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
        RequestTypePolicyInfo stPolicyInfo;
        std::string request;
        int priority;
        std::string type;

        if (elements["request"].asString(request) == CONV_OK)
        {
            auto it = mRequestNameToType.find(request);
            if (it != mRequestNameToType.end())
            {
                stPolicyInfo.request = it->second;
            }
            else
            {
                PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, \
                        "loadRequestPolicyJsonConfig: Invalid request type entry in config file, skipping");
                continue;
            }
        }
        stPolicyInfo.priority = elements["priority"].asNumber<int>();
        if (elements["type"].asString(type) == CONV_OK)
            stPolicyInfo.type = type;
        mAFRequestPolicyInfo[stPolicyInfo.request] = stPolicyInfo;
    }

    return true;
}

void AudioFocusManager::printRequestPolicyJsonInfo()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"printRequestPolicyJsonInfo");
    for (const auto& it : mAFRequestPolicyInfo)
    {
        const RequestTypePolicyInfo& policyInfo = it.second;
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "RequestType: %s  Priority: %d Type: %s", mRequestTypeToName[policyInfo.request].c_str(), \
                policyInfo.priority, policyInfo.type.c_str());
    }
}

/*
Functionality of this method:
->Registers the service with lunabus.
->Registers the methods with lunabus as public methods.
*/
bool AudioFocusManager::audioServiceRegister(std::string serviceName, GMainLoop *mainLoop, LSHandle **mServiceHandle)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"audioServiceRegister");
    bool bRetVal;
    LSError mLSError;
    LSErrorInit(&mLSError);
    //Register palm service
    bRetVal = LSRegister(serviceName.c_str(), mServiceHandle, &mLSError);
    LSERROR_CHECK_AND_PRINT(bRetVal);
    //Gmain attach
    bRetVal = LSGmainAttach(*mServiceHandle, mainLoop, &mLSError);
    LSERROR_CHECK_AND_PRINT(bRetVal);
    // add root category
    bRetVal = LSRegisterCategory (*mServiceHandle, "/",
                                 rootMethod, NULL, NULL, &mLSError);
    LSERROR_CHECK_AND_PRINT(bRetVal);
    if (!LSCategorySetData(*mServiceHandle, "/", this, &mLSError))
    LSERROR_CHECK_AND_PRINT(bRetVal);
    return true;
}

/*
Functionality of this method:
->Parses the payload.
->If resource is free, it will grant the resource to the requested application.
->If resource is not free, it invokes the appropriate method based on the request type.
*/
bool AudioFocusManager::requestAudioControl(LSHandle *sh, LSMessage *message, void *data)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"requestAudioControl");

    const char* request_payload = LSMessageGetPayload(message);
    pbnjson::JSchemaFragment inputSchema(requestAudioControlschema);
    pbnjson::JDomParser parser(NULL);

    if (!parser.parse(request_payload, inputSchema, NULL))
    {
        if (!returnErrorText(sh, message, "Invalid schema", AF_ERR_CODE_INVALID_SCHEMA))
        {
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestAudioControl: Failed to sent error text for Invalid Schema");
        }
        return true;
    }

    pbnjson::JValue root = parser.getDom();
    std::string sessionId = root["sessionId"].asString();
    std::string requestName = root["requestType"].asString();
    bool subscription = root["subscribe"].asBool();

    //TODO: Add validation logic for validation of session ID
    if (sessionId == "")
    {
        if (!returnErrorText(sh, message, "sessionId cannot be empty", AF_ERR_CODE_INTERNAL))
        {
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestAudioControl: Failed to sent error text for Internal Error \
                    due to empty sessionId");
        }
        return true;
    }

    const char* appId = LSMessageGetApplicationID(message);
    if (appId == NULL)
    {
        appId = LSMessageGetSenderServiceName(message);
        if (appId == NULL)
        {
            if (!returnErrorText(sh, message, "appId received as NULL", AF_ERR_CODE_INTERNAL))
            {
                PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestAudioControl: Failed to sent error text for Internal Error \
                        due to NULL appId");

            }
            return true;
        }
    }

    common::RequestType requestType;
    const auto& it = mRequestNameToType.find(requestName);
    if (it != mRequestNameToType.end())
    {
        requestType = it->second;
    }
    else
    {
        if (!returnErrorText(sh, message, "Invalid Request Type", AF_ERR_CODE_UNKNOWN_REQUEST))
        {
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestAudioControl: Failed to sent error text for Invalid \
                    Request Type to %s", appId);

        }
        return true;
    }

    if (!subscription)
    {
        if (!returnErrorText(sh, message, "Subscription should be true", AF_ERR_CODE_INTERNAL))
        {
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestAudioControl: Failed to sent error text for Internal Error \
                    due to incorrect subscription value to %s", appId);
        }
        return true;
    }

    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "requestAudioControl: sessionId: %s requestType: %s appId %s", \
            sessionId.c_str(), mRequestTypeToName[requestType].c_str(), appId);

    //Check if request type is already granted for the resource
    if (checkGrantedAlready(sh, message, appId, sessionId, requestType))
    {
        return true;
    }

    if (!checkFeasibility(sessionId, requestType))
    {
        //Reply CANNOT_BE_GRANTED
        if (!sendSignal(sh, message, "AF_CANNOTBEGRANTED", NULL))
        {
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestAudioControl: sendSignal failed : AF_CANNOTBEGRANTED");
        }

        return true;
    }

    if (!updateCurrentAppStatus(sessionId, requestType))
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestAudioControl: updateCurrentAppStatus Failed");
    }

    if (!sendSignal(sh, message, "AF_GRANTED", NULL))
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestAudioControl: sendSignal failed : AF_GRANTED");
        //TODO: Add failover steps, update paused list if needed
        return true;
    }

    if (LSMessageIsSubscription(message))
    {
        LSSubscriptionAdd(sh, "AFSubscriptionlist", message, NULL);
    }

    updateSessionActiveAppList(sessionId, appId, requestType);
    broadcastStatusToSubscribers();
    return true;
}

/*
Functionality of this method:
->Checks whether the incoming request is duplicate request or not.
->If it is a duplicate request sends AF_GRANTEDALREADY event to the app.
*/
bool AudioFocusManager::checkGrantedAlready(LSHandle *sh, LSMessage *message, std::string applicationId, const std::string& sessionId, common::RequestType& requestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkGrantedAlready");
    auto it = mSessionInfoMap.find(sessionId);
    if (it == mSessionInfoMap.end())
    {
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"Request from a new session %s", sessionId.c_str());
        return false;
    }

    SessionInfo& sessionInfo = it->second;
    for (auto itPaused = sessionInfo.pausedAppList.begin(); \
            itPaused != sessionInfo.pausedAppList.end(); itPaused++)
    {
        if (itPaused->request == requestType && \
                itPaused->appId == applicationId)
        {
            if(!sendSignal(sh, message, "AF_GRANTEDALREADY", NULL))
            {
                //unsubscribe the app
                if (!unsubscribingApp(applicationId))
                {
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "checkGrantedAlready: unsubscribing failed for %s", applicationId.c_str());
                }
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
        if (itActive->request == requestType && \
                itActive->appId == applicationId)
        {
            if(!sendSignal(sh, message, "AF_GRANTEDALREADY", NULL))
            {
                //unsubscribe the app
                if (!unsubscribingApp(applicationId))
                {
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "checkGrantedAlready: unsubscribing failed for %s", applicationId.c_str());
                }
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
bool AudioFocusManager::checkFeasibility(const std::string& sessionId, common::RequestType newRequestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility");
    auto itNewrequestPolicy = mAFRequestPolicyInfo.find(newRequestType);
    if (itNewrequestPolicy == mAFRequestPolicyInfo.end())
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: New request ty[e policy cannot be found");
        return false;
    }
    RequestTypePolicyInfo& newRequestPolicy = itNewrequestPolicy->second;
    auto itSession = mSessionInfoMap.find(sessionId);
    if (itSession == mSessionInfoMap.end())
    {
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: New session entry for : %s. Request feasible", sessionId.c_str());
        return true;
    }

    SessionInfo& curSessionInfo = itSession->second;

    //Check feasiblity in activeAppList
    for (const auto& itActive : curSessionInfo.activeAppList)
    {
        common::RequestType activeRequest = itActive.request;
        auto itActiveRequestPolicy = mAFRequestPolicyInfo.find(activeRequest);
        if (itActiveRequestPolicy == mAFRequestPolicyInfo.end())
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: Invalid request type entry. Skipping...");
            continue;

        }
        RequestTypePolicyInfo& activeRequestPolicy = itActiveRequestPolicy->second;

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
                    mRequestTypeToName[newRequestPolicy.request].c_str(), mRequestTypeToName[activeRequestPolicy.request].c_str());
            return false;
        }
    }

    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: New requestType: %s shall be approved", \
            mRequestTypeToName[newRequestPolicy.request].c_str());
    return true;
}

bool AudioFocusManager::updateCurrentAppStatus(const std::string& sessionId, common::RequestType newRequest)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus");

    auto itNewRequestPolicy = mAFRequestPolicyInfo.find(newRequest);
    if (itNewRequestPolicy == mAFRequestPolicyInfo.end())
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: New request policy cannot be found");
        return false;
    }

    RequestTypePolicyInfo& newRequestPolicy = itNewRequestPolicy->second;
    auto itSession = mSessionInfoMap.find(sessionId);
    if (itSession == mSessionInfoMap.end())
    {
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: new session requested: %s No need to update app status", \
                sessionId.c_str());
        return true;
    }

    SessionInfo& curSessionInfo = itSession->second;

    //Update active app list based on new request
    for (auto itActive = curSessionInfo.activeAppList.begin(); itActive != curSessionInfo.activeAppList.end(); itActive++)
    {
        const auto& itActiveRequestpolicy = mAFRequestPolicyInfo.find(itActive->request);
        if (itActiveRequestpolicy == mAFRequestPolicyInfo.end())
        {
            PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Invalid requestType in active list for %s. Skipping", \
                    sessionId.c_str());
            continue;
        }
        RequestTypePolicyInfo& activeRequestpolicy = itActiveRequestpolicy->second;

        if (activeRequestpolicy.priority >= newRequestPolicy.priority)
        {
            if (newRequestPolicy.type == "short" && \
                    (activeRequestpolicy.type == "long" || activeRequestpolicy.type == "mix"))
            {
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: send AF_PAUSE to %s", \
                        itActive->appId.c_str());
                if (!signalToApp(itActive->appId, "AF_PAUSE"))
                {
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to sent AF_PAUSE to %s", \
                            itActive->appId.c_str());
                    if (!unsubscribingApp(itActive->appId))
                    {
                        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to Unsubscribe app: %s", \
                                itActive->appId.c_str());
                    }
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
                {
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to sent AF_LOST to %s", \
                            itActive->appId.c_str());
                }
                if (!unsubscribingApp(itActive->appId))
                {
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to Unsubscribe to active app: %s", \
                            itActive->appId.c_str());
                }

                curSessionInfo.activeAppList.erase(itActive--);
            }
            else
            {
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: App can mix and play %s", \
                        itActive->appId.c_str());

            }

        }
    }

    //Update paused apps list based on new request
    for (auto itPaused = curSessionInfo.pausedAppList.begin(); itPaused != curSessionInfo.pausedAppList.end(); itPaused++)
    {
        const auto& itPausedRequestpolicy = mAFRequestPolicyInfo.find(itPaused->request);
        if (itPausedRequestpolicy == mAFRequestPolicyInfo.end())
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
                    {
                        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to Unsubscribe to paused app");
                    }
            }
            if (!unsubscribingApp(itPaused->appId))
            {
                PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: unsubscribing failed");
            }
            curSessionInfo.pausedAppList.erase(itPaused--);
        }

    }
    return true;
}

/*Functionality of this methos:
 * -> Update the session active app list if session already present
 *  ->Create new session Info and update active app list
 */
void AudioFocusManager::updateSessionActiveAppList(const std::string& sessionId, const std::string appId, common::RequestType requestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateSessionAppList: sessionId: %s", sessionId.c_str());
    AppInfo newAppInfo;
    newAppInfo.appId = appId;
    newAppInfo.request = requestType;

    auto itSession = mSessionInfoMap.find(sessionId);
    if (itSession == mSessionInfoMap.end())
    {
        SessionInfo newSessionInfo;
        newSessionInfo.activeAppList.push_back(newAppInfo);
        newSessionInfo.sessionId = sessionId;
        mSessionInfoMap[sessionId] = newSessionInfo;
        PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateSessionActiveAppList: new session details added. Session: %s", \
                sessionId.c_str());
        return;
    }

    SessionInfo& sessionInfo = itSession->second;
    sessionInfo.activeAppList.push_back(newAppInfo);

}

/*
Functionality of this method:
->Sends AF_LOST event to the current running application and unsubscribes it.
->If sending AF_LOST event is fails, it will return AF_CANNOTBEGRANTED to the newly requesting app
  and will unsubscribe it.
*/
bool AudioFocusManager::sendLostMessage()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"sendLostMessage");
    return true;
}

/*
Functionality of this method:
->Sends AF_PAUSE event to the current running application.
->If sending AF_PAUSE event fails, it will return AF_CANNOTBEGRANTED to the newly requesting app
  and will unsubscribe it.
*/
bool AudioFocusManager::sendPauseMessage()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"sendPauseMessage");
    return true;
}

/*
Functionality of this method:
->This will unsubscribe the app and removes its entry.
->Sends resume event to the application which is in paused state if there are any such applications.
*/
bool AudioFocusManager::releaseAudioControl(LSHandle *sh, LSMessage *message, void *data)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"releaseAudioControl");
    const char* payload = LSMessageGetPayload(message);
    pbnjson::JSchemaFragment inputSchema(getStatusschema);
    pbnjson::JDomParser parser(NULL);
    LSError lserror;
    LSErrorInit (&lserror);

    if (!parser.parse(payload, inputSchema, NULL))
    {
        if (!returnErrorText(sh, message, "Invalid schema", AF_ERR_CODE_INVALID_SCHEMA))
        {
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "releaseAudioControl: Failed to sent error text for Invalid Schema");
        }
        return true;
    }

    pbnjson::JValue root = parser.getDom();
    std::string sessionId = root["sessionId"].asString();

    const char* appId = LSMessageGetApplicationID(message);
    if (appId == NULL)
    {
        appId = LSMessageGetSenderServiceName(message);
        if (appId == NULL)
        {
            if (!returnErrorText(sh, message, "Internal error", AF_ERR_CODE_INTERNAL))
            {
                PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "releaseAudioControl: Failed to sent error text for Internal Error \
                        due to NULL appId to %s", appId);
            }
            return true;
        }
    }
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"releaseAudioControl: sessionId: %s appId: %s", sessionId.c_str(), appId);

    auto itSession = mSessionInfoMap.find(sessionId);
    if (itSession == mSessionInfoMap.end())
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"releaseAudioControl: Session ID cannot be find");
        if (!returnErrorText(sh, message, "Session Id not found", AF_ERR_CODE_INTERNAL))
        {
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "releaseAudioControl: Failed to sent error text for Internal Error \
                    due to session ID not found");
        }
        return true;
    }

    SessionInfo& curSessionInfo = itSession->second;

    for (auto itPaused = curSessionInfo.pausedAppList.begin(); itPaused != curSessionInfo.pausedAppList.end(); itPaused++)
    {
        if (itPaused->appId == appId)
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"releaseAudioControl: Removing appId: %s Request type: %s", \
                    appId, mRequestTypeToName[itPaused->request].c_str());
            if (!unsubscribingApp(appId))
            {
                PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "releaseAudioControl: unsubscribingApp failed for %s", appId);
            }
            curSessionInfo.pausedAppList.erase(itPaused--);
            if (!sendSignal(sh, message, "AF_SUCCESSFULLY_RELEASED", NULL))
            {
                PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "releaseAudioControl: Failed to sent message: AF_SUCCESSFULLY_RELEASED");
            }
            return true;
        }
    }

    for (auto itActive = curSessionInfo.activeAppList.begin(); itActive != curSessionInfo.activeAppList.end(); itActive++)
    {
        if (itActive->appId == appId)
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"releaseAudioControl: Removing appId: %s Request type: %s", \
                    appId, mRequestTypeToName[itActive->request].c_str());
            if (!unsubscribingApp(appId))
            {
                PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "releaseAudioControl: unsubscribingApp failed for %s", appId);
            }
            curSessionInfo.activeAppList.erase(itActive--);
            updatePausedAppStatus(curSessionInfo, itActive->request);
            if (!sendSignal(sh, message, "AF_SUCCESSFULLY_RELEASED", NULL))
            {
                PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "releaseAudioControl: Failed to sent message: AF_SUCCESSFULLY_RELEASED");
            }
            return true;
        }
    }

    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"releaseAudioControl: appId: %s cannot be found in session: %s" , \
            appId, curSessionInfo.sessionId.c_str());
    if (!returnErrorText(sh, message, "Application not registered", AF_ERR_CODE_INTERNAL))
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "releaseAudioControl: Failed to sent error text for Internal Error: \
                application not registered");
    }
    return true;
}

/*Functionality of this method:
 * To update the paused app list in the session based on the removed request type policy
 */
void AudioFocusManager::updatePausedAppStatus(SessionInfo& sessionInfo, common::RequestType removedRequest)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updatePausedAppStatus: Request Type: %s", mRequestTypeToName[removedRequest].c_str());

    auto requestInfoIt = mAFRequestPolicyInfo.find(removedRequest);
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
        auto activeRequestInfo = mAFRequestPolicyInfo.find(activeApp.request);
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
    const char* payload = LSMessageGetPayload(message);
    pbnjson::JSchemaFragment inputSchema(getStatusschema);
    pbnjson::JDomParser parser(NULL);
    LSError lserror;
    LSErrorInit (&lserror);

    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"%s : %d", __FUNCTION__, __LINE__);
    if (!parser.parse(payload, inputSchema, NULL))
    {
        if (!returnErrorText(sh, message, "Invalid schema", AF_ERR_CODE_INVALID_SCHEMA))
        {
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "getStatus: Failed to sent error text for Invalid Schema");
        }
        return true;
    }
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"%s : %d", __FUNCTION__, __LINE__);

    pbnjson::JValue root = parser.getDom();
    bool subscription = root["subscribe"].asBool();
    if (LSMessageIsSubscription (message))
    {
        if (!LSSubscriptionProcess(sh, message, &subscription, &lserror))
        {
            LSErrorPrintAndFree(&lserror);
        }
    }

    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"%s : %d", __FUNCTION__, __LINE__);
    if (!sendSignal(sh, message, getAudioFocusManagerStatusPayload(), NULL))
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"getStatus: sendSignal failed");

    }
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"getStatus: Succesfully sent response");
    return true;
}

/*Functionality of this method
 * TO get the JSON payload for getStatus response in string format
 */
std::string AudioFocusManager::getAudioFocusManagerStatusPayload()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"getAudioFocusManagerStatusPayload");

    pbnjson::JValue audioFocusStatus = pbnjson::JObject();
    pbnjson::JArray sessionsList = pbnjson::JArray();
    for (auto sessionInfomap : mSessionInfoMap)
    {
        SessionInfo& sessionInfo = sessionInfomap.second;
        pbnjson::JArray activeAppArray = pbnjson::JArray();
        pbnjson::JArray pausedAppArray = pbnjson::JArray();
        pbnjson::JValue curSession = pbnjson::JObject();

        curSession.put("sessionId",sessionInfo.sessionId);
        for (auto activeAppInfo : sessionInfo.activeAppList)
        {
            pbnjson::JValue activeApp = pbnjson::JObject();
            activeApp.put("appId", activeAppInfo.appId);
            activeApp.put("requestType", mRequestTypeToName[activeAppInfo.request]);
            activeAppArray.append(activeApp);
        }
        curSession.put("AFactiveRequests",activeAppArray);

        for (auto pausedAppInfo : sessionInfo.pausedAppList)
        {
            pbnjson::JValue pausedApp = pbnjson::JObject();
            pausedApp.put("appId", pausedAppInfo.appId);
            pausedApp.put("requestType", mRequestTypeToName[pausedAppInfo.request]);
            pausedAppArray.append(pausedApp);
        }
        curSession.put("pausedRequests", pausedAppArray);
        sessionsList.append(curSession);
    }

    audioFocusStatus.put("audioFocusStatus", sessionsList);
    return audioFocusStatus.stringify();
}

/*
 * Functionality of this method:
 * Notigy /getStatus subscribers on the current AudioFocusManager status
 */
void AudioFocusManager::broadcastStatusToSubscribers()
{
    LSError lserror;
    LSErrorInit(&lserror);
    std::string reply = getAudioFocusManagerStatusPayload();
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"broadcastStatusToSubscribers: reply message to subscriber: %s", \
            reply.c_str());
    //TODO: This is deprecated method for accessing LsHandle.
    //Once the entire LS subscription mechanism is updated for the new API sets this also will be upgraded.
    LSHandle *shHandle = mServiceHandle;
    if (!shHandle)
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"broadcastStatusToSubscriber:: failed to get lsHandle");
    }
    if (!LSSubscriptionReply(shHandle, AF_API_GET_STATUS, reply.c_str(), &lserror))
    {
        LSErrorPrintAndFree(&lserror);
    }
}

/*
Functionality of this method:
->It will send events like AF_LOST/AF_PAUSE to the current running application.
->It will send resume(AF_GRANTED) event to the paused application.
*/
bool AudioFocusManager::signalToApp(const std::string& applicationId, const std::string& signalMessage)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"signalToApp");
    //TODO: Implement LS Message
    return true;
    LSHandle *serviceHandlePrivate = NULL;

    if(subscriptionUtility(applicationId, mServiceHandle, 's', signalMessage))
    {
        return true;
    }

    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"signalToApp:failed");
    return false;
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
    jsonObject.put("errorCode", 0);
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

    if(!LSMessageReply(sh, message, pbnjson::JGenerator::serialize(jsonObject, pbnjson::JSchemaFragment("{}")).c_str(), NULL))
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

    if(subscriptionUtility(applicationId, mServiceHandle, 'r', ""))
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
    LSHandle *serviceHandlePrivate = NULL;

    if(subscriptionUtility(applicationId, mServiceHandle, 'c', ""))
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
    broadcastLostToAll(mServiceHandle);
    return true;
}

/*
Functionality of this method:
->This will send lost message (AF_LOST) to all the app(s) which is/are using the resource and also to the
  paused applications.
*/
bool AudioFocusManager::broadcastLostToAll(LSHandle *serviceHandle)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"broadcastLostToAll");
    std::string subscribed_appId;
    pbnjson::JDomParser parser(NULL);
    LSSubscriptionIter *iter = NULL;
    bool retValAcquire = false;
    LSError lserror;
    LSErrorInit(&lserror);
    retValAcquire = LSSubscriptionAcquire(serviceHandle, "AFSubscriptionlist", &iter, &lserror);

    if(retValAcquire)
    {
        pbnjson::JValue jsonObject = pbnjson::Object();
        jsonObject.put("errorCode", 0);
        jsonObject.put("returnValue", true);
        jsonObject.put("result", "AF_LOST");

        while(LSSubscriptionHasNext(iter))
        {
            LSMessage *iter_message = LSSubscriptionNext(iter);
            const char* msgJsonArgument = LSMessageGetPayload(iter_message);
            parser.parse(msgJsonArgument, pbnjson::JSchemaFragment("{}"), NULL);
            pbnjson::JValue msgJsonSubArgument = parser.getDom();
            subscribed_appId = msgJsonSubArgument["appId"].asString();

            if(!LSMessageReply(serviceHandle, iter_message, pbnjson::JGenerator::serialize(jsonObject, pbnjson::JSchemaFragment("{}")).c_str(), &lserror))
            {
                PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"broadcastLostToAll: LSMessageReply Failed");
            }

            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"broadcastLostToAll: AF_LOST sent to %s", subscribed_appId.c_str());
            LSSubscriptionRemove(iter);
        }

        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
        LSSubscriptionRelease(iter);
        return true;
    }
    else
    {
        return false;
    }
}
