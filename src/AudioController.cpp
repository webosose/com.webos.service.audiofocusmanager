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
#include <AudioController.h>

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

LSPalmService *AudioController::mServiceHandle;

LSMethod AudioController::rootMethod[] = {
    {"requestAudioControl", AudioController::_requestAudioControl},
    {"releaseAudioControl", AudioController::_releaseAudioControl},
    {"getStatus", AudioController::_getStatus},
    {0, 0}
};

AudioController *AudioController::ACService = NULL;

AudioController::AudioController()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "AudioController Constructor invoked");
}

/*
Functionality of this method:
->This function will create the object for audiocontroller if not present and returns the pointer.
->If the object is already created then it will just return the pointer.
*/
AudioController *AudioController::getInstance()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "getInstance");
    if(NULL == ACService)
    {
        ACService = new(std::nothrow) AudioController();
        return ACService;
    }
    else
    {
        return ACService;
    }
}

/*
Functionality of this method:
->This function will delete the object for audiocontroller if present.
->If the object is not present then it will do nothing.
*/
void AudioController::deleteInstance()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "deleteInstance");
    if(ACService)
    {
        delete ACService;
        ACService = NULL;
    }
    return;
}

/*
Functionality of this method:
->Initializes the service registration.
*/
bool AudioController::init(GMainLoop *mainLoop)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"init");

    if(audioServiceRegister("com.lge.audiocontroller", mainLoop, &mServiceHandle) == false)
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "com.lge.audiocontroller service registration failed");
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

void AudioController::createMapRequestNametoType()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "createMapRequestNametoType");

    mRequestNameToType["ACREQUEST_GAIN"] = common::RequestType::GAIN;
    mRequestNameToType["ACREQUEST_MUTE_LONG"] = common::RequestType::MUTE_LONG;
    mRequestNameToType["ACREQUEST_MUTE_SHORT"] = common::RequestType::MUTE_SHORT;
    mRequestNameToType["ACREQUEST_CALL"] = common::RequestType::CALL;
    mRequestNameToType["ACREQUEST_VOICE"] = common::RequestType::MUTE_VOICE;
    mRequestNameToType["ACREQUEST_MUTE_MUTUAL"] = common::RequestType::MUTE_MUTUAL;
    mRequestNameToType["ACREQUEST_TRANSIENT"] = common::RequestType::TRANSIENT;
}

void AudioController::createMapRequestTypetoName()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "createMapRequestTypetoName");

    mRequestTypeToName[common::RequestType::GAIN] = "ACREQUEST_GAIN";
    mRequestTypeToName[common::RequestType::MUTE_LONG] = "ACREQUEST_MUTE_LONG";
    mRequestTypeToName[common::RequestType::MUTE_SHORT] = "ACREQUEST_MUTE_SHORT";
    mRequestTypeToName[common::RequestType::CALL] = "ACREQUEST_CALL";
    mRequestTypeToName[common::RequestType::MUTE_VOICE] = "ACREQUEST_VOICE";
    mRequestTypeToName[common::RequestType::MUTE_MUTUAL] = "ACREQUEST_MUTE_MUTUAL";
    mRequestTypeToName[common::RequestType::TRANSIENT] = "ACREQUEST_TRANSIENT";
}

/*
 * Functionality of this method:
 * ->Load the RequestPolicy JSON config and populate internal structure for request info
 */
bool AudioController::loadRequestPolicyJsonConfig()
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
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "Failed to parse json config file, using defaults. File: %s", jsonFilePath.str());
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
        mACRequestPolicyInfo[stPolicyInfo.request] = stPolicyInfo;
    }

    return true;
}

void AudioController::printRequestPolicyJsonInfo()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"printRequestPolicyJsonInfo");
    for (const auto& it : mACRequestPolicyInfo)
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
bool AudioController::audioServiceRegister(char *srvcname, GMainLoop *mainLoop, LSPalmService **msvcHandle)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"audioServiceRegister");
    bool bRetVal;
    LSError mLSError;
    LSErrorInit(&mLSError);
    //Register palm service
    bRetVal = LSRegisterPalmService(srvcname, msvcHandle, &mLSError);
    LSERROR_CHECK_AND_PRINT(bRetVal);
    //Gmain attach
    bRetVal = LSGmainAttachPalmService(*msvcHandle, mainLoop, &mLSError);
    LSERROR_CHECK_AND_PRINT(bRetVal);
    // add root category
    bRetVal = LSPalmServiceRegisterCategory(*msvcHandle, "/", rootMethod, NULL, NULL, this, &mLSError);
    LSERROR_CHECK_AND_PRINT(bRetVal);
    return true;
}

/*
Functionality of this method:
->Parses the payload.
->If resource is free, it will grant the resource to the requested application.
->If resource is not free, it invokes the appropriate method based on the request type.
*/
bool AudioController::requestAudioControl(LSHandle *sh, LSMessage *message, void *data)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"requestAudioControl");

    const char* request_payload = LSMessageGetPayload(message);
    pbnjson::JSchemaFragment inputSchema(requestAudioControlschema);
    pbnjson::JDomParser parser(NULL);

    if (!parser.parse(request_payload, inputSchema, NULL))
    {
        if (!returnErrorText(sh, message, "Invalid schema", AC_ERR_CODE_INVALID_SCHEMA))
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
        if (!returnErrorText(sh, message, "sessionId cannot be empty", AC_ERR_CODE_INTERNAL))
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
            if (!returnErrorText(sh, message, "appId received as NULL", AC_ERR_CODE_INTERNAL))
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
        if (!returnErrorText(sh, message, "Invalid Request Type", AC_ERR_CODE_UNKNOWN_REQUEST))
        {
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestAudioControl: Failed to sent error text for Invalid \
                    Request Type to %s", appId);

        }
        return true;
    }

    if (!subscription)
    {
        if (!returnErrorText(sh, message, "Subscription should be true", AC_ERR_CODE_INTERNAL))
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
        if (!sendSignal(sh, message, "AC_CANNOTBEGRANTED", NULL))
        {
            PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestAudioControl: sendSignal failed : AC_CANNOTBEGRANTED");
        }

        return true;
    }

    if (!updateCurrentAppStatus(sessionId, requestType))
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestAudioControl: updateCurrentAppStatus Failed");
    }

    if (!sendSignal(sh, message, "AC_GRANTED", NULL))
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "requestAudioControl: sendSignal failed : AC_GRANTED");
        //TODO: Add failover steps
        return true;
    }

    if (LSMessageIsSubscription(message))
    {
        LSSubscriptionAdd(sh, "ACSubscriptionlist", message, NULL);
    }

    updateSessionActiveAppList(sessionId, appId, requestType);
    broadcastStatusToSubscribers();
    return true;
}

/*
Functionality of this method:
->Checks whether the incoming request is duplicate request or not.
->If it is a duplicate request sends AC_GRANTEDALREADY event to the app.
*/
bool AudioController::checkGrantedAlready(LSHandle *sh, LSMessage *message, std::string applicationId, const std::string& sessionId, common::RequestType& requestType)
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
            if(!sendSignal(sh, message, "AC_GRANTEDALREADY", NULL))
            {
                //unsubscribe the app
                if (!unsubscribingApp(applicationId))
                {
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "checkGrantedAlready: unsubscribing failed for %s", applicationId.c_str());
                }
                sessionInfo.pausedAppList.erase(itPaused--);
                PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "checkGrantedAlready: Failed to send AC_GRANTEDALREADY %s", applicationId.c_str());
                return true;

            }
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkGrantedAlready: AC_GRANTEDALREADY %s", applicationId.c_str());
            return true;
        }
    }

    for (auto itActive = sessionInfo.activeAppList.begin(); \
            itActive != sessionInfo.activeAppList.end(); itActive++)
    {
        if (itActive->request == requestType && \
                itActive->appId == applicationId)
        {
            if(!sendSignal(sh, message, "AC_GRANTEDALREADY", NULL))
            {
                //unsubscribe the app
                if (!unsubscribingApp(applicationId))
                {
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "checkGrantedAlready: unsubscribing failed for %s", applicationId.c_str());
                }
                sessionInfo.activeAppList.erase(itActive--);
                PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "checkGrantedAlready: Failed to send AC_GRANTEDALREADY %s", applicationId.c_str());
                return true;

            }
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkGrantedAlready: AC_GRANTEDALREADY");
            return true;
        }
    }

    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT, "checkGrantedAlready: Request shall be approved for %s", applicationId.c_str());
    return false;
}

/*Functionality of this method:
 * To check if active request types in the requesting session has any request which will not grant the new request type*/
bool AudioController::checkFeasibility(const std::string& sessionId, common::RequestType newRequestType)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility");
    auto itNewrequestPolicy = mACRequestPolicyInfo.find(newRequestType);
    if (itNewrequestPolicy == mACRequestPolicyInfo.end())
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
        auto itActiveRequestPolicy = mACRequestPolicyInfo.find(activeRequest);
        if (itActiveRequestPolicy == mACRequestPolicyInfo.end())
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: Invalid request type entry. Skipping...");
            continue;

        }
        RequestTypePolicyInfo& activeRequestPolicy = itActiveRequestPolicy->second;

        if (activeRequestPolicy.priority < newRequestPolicy.priority && \
                activeRequestPolicy.type != "mix")
        {
            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: newrequestType: %s cannot be granted due to active request: %s", \
                    mRequestTypeToName[newRequestPolicy.request].c_str(), mRequestTypeToName[activeRequestPolicy.request].c_str());
            return false;
        }
    }

    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkFeasibility: New requestType: %s shall be approved", \
            mRequestTypeToName[newRequestPolicy.request].c_str());
    return true;
}

bool AudioController::updateCurrentAppStatus(const std::string& sessionId, common::RequestType newRequest)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus");

    auto itNewRequestPolicy = mACRequestPolicyInfo.find(newRequest);
    if (itNewRequestPolicy == mACRequestPolicyInfo.end())
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
        const auto& itActiveRequestpolicy = mACRequestPolicyInfo.find(itActive->request);
        if (itActiveRequestpolicy == mACRequestPolicyInfo.end())
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
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: send AC_PAUSE to %s", \
                        itActive->appId.c_str());
                if (!signalToApp(itActive->appId, "AC_PAUSE"))
                {
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to sent AC_PAUSE to %s", \
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
                PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: send AC_LOST to %s", \
                        itActive->appId.c_str());

                if (!signalToApp(itActive->appId, "AC_LOST"))
                {
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to sent AC_LOST to %s", \
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
        const auto& itPausedRequestpolicy = mACRequestPolicyInfo.find(itPaused->request);
        if (itPausedRequestpolicy == mACRequestPolicyInfo.end())
        {
            PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Invalid requestType in paused list for %s. Skipping", \
                    sessionId.c_str());
            continue;
        }

        if (newRequestPolicy.type == "long")
        {
            if (!signalToApp(itPaused->appId, "AC_LOST"))
            {
                    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"updateCurrentAppStatus: Failed to sent AC_LOST to %s", \
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
void AudioController::updateSessionActiveAppList(const std::string& sessionId, const std::string appId, common::RequestType requestType)
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
->Sends AC_LOST event to the current running application and unsubscribes it.
->If sending AC_LOST event is fails, it will return AC_CANNOTBEGRANTED to the newly requesting app
  and will unsubscribe it.
*/
bool AudioController::sendLostMessage()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"sendLostMessage");
    return true;
}

/*
Functionality of this method:
->Sends AC_PAUSE event to the current running application.
->If sending AC_PAUSE event fails, it will return AC_CANNOTBEGRANTED to the newly requesting app
  and will unsubscribe it.
*/
bool AudioController::sendPauseMessage()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"sendPauseMessage");
    return true;
}

/*
Functionality of this method:
->This will unsubscribe the app and removes its entry.
->Sends resume event to the application which is in paused state if there are any such applications.
*/
bool AudioController::releaseAudioControl(LSHandle *sh, LSMessage *message, void *data)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"releaseAudioControl");
    return true;

}

/*
Functionality of this method:
->This will return the current status of granted request types in audiocontroller for each session
*/
bool AudioController::getStatus(LSHandle *sh, LSMessage *message, void *data)
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
        if (!returnErrorText(sh, message, "Invalid schema", AC_ERR_CODE_INVALID_SCHEMA))
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
    if (!sendSignal(sh, message, getAudioControllerStatusPayload(), NULL))
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"getStatus: sendSignal failed");

    }
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"getStatus: Succesfully sent response");
    return true;
}

/*Functionality of this method
 * TO get the JSON payload for getStatus response in string format
 */
std::string AudioController::getAudioControllerStatusPayload()
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"getAudioControllerStatusPayload");

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
        curSession.put("activeRequests",activeAppArray);

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
 * Notigy /getStatus subscribers on the current audiocontroller status
 */
void AudioController::broadcastStatusToSubscribers()
{
    LSError lserror;
    LSErrorInit(&lserror);
    std::string reply = getAudioControllerStatusPayload();
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"broadcastStatusToSubscribers: reply message to subscriber: %s", \
            reply.c_str());
    //TODO: This is deprecated method for accessing LsHandle.
    //Once the entire LS subscription mechanism is updated for the new API sets this also will be upgraded.
    LSHandle *shHandle = LSPalmServiceGetPrivateConnection(mServiceHandle);
    if (!shHandle)
    {
        shHandle = LSPalmServiceGetPublicConnection(mServiceHandle);
        if (!shHandle)
        {
            PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"broadcastStatusToSubscriber:: failed to get lsHandle");
            return;
        }
    }
    if (!LSSubscriptionReply(shHandle, AC_API_GET_STATUS, reply.c_str(), &lserror))
    {
        LSErrorPrintAndFree(&lserror);
    }
}

/*
Functionality of this method:
->It will send events like AC_LOST/AC_PAUSE to the current running application.
->It will send resume(AC_GRANTED) event to the paused application.
*/
bool AudioController::signalToApp(const std::string& applicationId, const std::string& signalMessage)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"signalToApp");
		//TODO: Implement LS Message
		return true;
    LSHandle *serviceHandlePrivate = NULL;
    serviceHandlePrivate = LSPalmServiceGetPrivateConnection(mServiceHandle);

    if(serviceHandlePrivate)
    {
        if(subscriptionUtility(applicationId, serviceHandlePrivate, 's', signalMessage))
        {
            return true;
        }
    }

    LSHandle *serviceHandlePublic = NULL;
    serviceHandlePublic = LSPalmServiceGetPublicConnection(mServiceHandle);

    if(serviceHandlePublic)
    {
        if(subscriptionUtility(applicationId, serviceHandlePublic, 's', signalMessage))
        {
            return true;
        }
    }

    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"signalToApp:failed");
    return false;
}

/*
Functionality of this method:
->This is a utility function used for dealing with subscription list.
->Based on the operation received it does the following functionalities:-
    operation 's' : Signals the corresponding application with the signalMessage(AC_LOST/AC_PAUSE/AC_GRANTED) passed to it.
                    It will send events like AC_LOST/AC_PAUSE to the current running application.
                    It will send resume(AC_GRANTED) event to the paused application.
    operation 'r' : Removes the subscription for the respective applicationId passed.
    operation 'c' : This is for checking whether the corresponding applicationId is subscribed or not.
*/
bool AudioController::subscriptionUtility(const std::string& applicationId, LSHandle *serviceHandle, const char operation, const std::string& signalMessage)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"subscriptionUtility: Operation: %c, appId: %s", operation, applicationId.c_str());
    std::string subscribed_appId;
    pbnjson::JDomParser parser(NULL);
    LSSubscriptionIter *iter = NULL;
    bool retValAcquire = false,found = false;
    LSError lserror;
    LSErrorInit(&lserror);
    retValAcquire = LSSubscriptionAcquire(serviceHandle, "ACSubscriptionlist", &iter, &lserror);

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

bool AudioController::sendSignal(LSHandle *serviceHandle, LSMessage *iter_message, const std::string& signalMessage, LSError *lserror)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"sendSignal");
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
bool AudioController::returnErrorText(LSHandle *sh, LSMessage *message, const std::string& errorText, int errorCode)
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
bool AudioController::unsubscribingApp(const std::string& applicationId)
{
    //TODO:Implement LS subscription
    return true;
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"unsubscribingApp: %s", applicationId.c_str());
    LSHandle *serviceHandlePrivate = NULL;
    serviceHandlePrivate = LSPalmServiceGetPrivateConnection(mServiceHandle);

    if(serviceHandlePrivate)
    {
        if(subscriptionUtility(applicationId, serviceHandlePrivate, 'r', ""))
        {
            return true;
        }
    }

    LSHandle *serviceHandlePublic = NULL;
    serviceHandlePublic = LSPalmServiceGetPublicConnection(mServiceHandle);

    if(serviceHandlePublic)
    {
        if(subscriptionUtility(applicationId, serviceHandlePublic, 'r', ""))
        {
            return true;
        }
    }

    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"unsubscribingApp: failed in LSSubscriptionAcquire");
    return false;
}

/*
Functionality of this method:
->This will check whether the application is alive or not.
*/
bool AudioController::checkSubscription(const std::string& applicationId)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"checkSubscription: %s", applicationId.c_str());
    LSHandle *serviceHandlePrivate = NULL;
    serviceHandlePrivate = LSPalmServiceGetPrivateConnection(mServiceHandle);

    if(serviceHandlePrivate)
    {
        if(subscriptionUtility(applicationId, serviceHandlePrivate, 'c', ""))
        {
            return true;
        }
    }

    LSHandle *serviceHandlePublic = NULL;
    serviceHandlePublic = LSPalmServiceGetPublicConnection(mServiceHandle);

    if(serviceHandlePublic)
    {
        if(subscriptionUtility(applicationId, serviceHandlePublic, 'c', ""))
        {
            return true;
        }
    }

    PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT,"checkSubscription: failed in LSSubscriptionAcquire %s", applicationId.c_str());
    return false;
}

/*
Functionality of this method:
->Prints the error message.
*/
void AudioController::LSErrorPrintAndFree(LSError *ptrLSError)
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
bool AudioController::signalTermCaught()
{
    LSHandle *serviceHandlePrivate = NULL;
    serviceHandlePrivate = LSPalmServiceGetPrivateConnection(mServiceHandle);

    if(serviceHandlePrivate)
    {
        broadcastLostToAll(serviceHandlePrivate);
    }

    LSHandle *serviceHandlePublic = NULL;
    serviceHandlePublic = LSPalmServiceGetPublicConnection(mServiceHandle);

    if(serviceHandlePublic)
    {
        broadcastLostToAll(serviceHandlePublic);
    }

    return true;
}

/*
Functionality of this method:
->This will send lost message (AC_LOST) to all the app(s) which is/are using the resource and also to the
  paused applications.
*/
bool AudioController::broadcastLostToAll(LSHandle *serviceHandle)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"broadcastLostToAll");
    std::string subscribed_appId;
    pbnjson::JDomParser parser(NULL);
    LSSubscriptionIter *iter = NULL;
    bool retValAcquire = false;
    LSError lserror;
    LSErrorInit(&lserror);
    retValAcquire = LSSubscriptionAcquire(serviceHandle, "ACSubscriptionlist", &iter, &lserror);

    if(retValAcquire)
    {
        pbnjson::JValue jsonObject = pbnjson::Object();
        jsonObject.put("errorCode", 0);
        jsonObject.put("returnValue", true);
        jsonObject.put("result", "AC_LOST");

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

            PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"broadcastLostToAll: AC_LOST sent to %s", subscribed_appId.c_str());
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
