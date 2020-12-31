/* @@@LICENSE
 *
 * Copyright (c) 2020 - 2021 LG Electronics, Inc.
 *
 * Confidential computer software. Valid license from LG required for
 * possession, use or copying. Consistent with FAR 12.211 and 12.212,
 * Commercial Computer Software, Computer Software Documentation, and
 * Technical Data for Commercial Items are licensed to the U.S. Government
 * under vendor's standard commercial license.
 *
 * LICENSE@@@
 */

#include <iostream>
#include <lunaservice.h>
#include <glib.h>
#include <audioFocusManager.h>
#include <signal.h>
#include <stdlib.h>
#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <sys/time.h>
#include <sys/resource.h>
#include <sstream>
#include "common.h"

#include "log.h"

static const char* const logContextName = "AudioFocusManager";
static const char* const logPrefix= "[AF]";

static GMainLoop *mainLoop = NULL;
static LSHandle *gServiceHandle = NULL;

//return palmservice handle of "com.webos.service.audio"
LSHandle *
GetLSService()
{
    return gServiceHandle;
}

/*
Functionality of this method:
->Registers the service with lunabus.
->Registers the methods with lunabus as public methods.
*/
bool audioServiceRegister(std::string srvcname, GMainLoop *mainLoop, LSHandle **msvcHandle)
{
    PM_LOG_INFO(MSGID_CORE, INIT_KVCOUNT,"audioServiceRegister");
    bool bRetVal;
    CLSError mLSError;
    //Register palm service
    if(!LSRegister(srvcname.c_str(), msvcHandle, &mLSError))
    {
        mLSError.Print(__FUNCTION__, __LINE__, G_LOG_LEVEL_CRITICAL);
        return false ;
    }
    //Gmain attach
    if(!LSGmainAttach(*msvcHandle, mainLoop, &mLSError))
    {
        mLSError.Print(__FUNCTION__, __LINE__, G_LOG_LEVEL_CRITICAL);
        return false ;
    }
    return true;
}

AudioFocusManager *audioFocusManager = NULL;

void signalHandler(int signal)
{
    PM_LOG_WARNING(MSGID_INIT, INIT_KVCOUNT, "Signal Caught");

    //TODO handle system signals
    if(mainLoop)
    {
        g_main_loop_quit(mainLoop);
        PM_LOG_INFO(MSGID_STUTDOWN, INIT_KVCOUNT,"signalHandler-> g_main_loop_quit(mainLoop)");
    }
    return;
}

void exit_proc(void)
{
    //TODO delete audiofocus manager object
    PM_LOG_INFO(MSGID_STUTDOWN, INIT_KVCOUNT, "deleted AudioFocusManager object");
}

int main(int argc, char *argv[])
{
    atexit(exit_proc);
    PmLogErr error = setPmLogContext(logContextName);
    if (error != kPmLogErr_None)
    {
        std::cerr << logPrefix << "Failed to setup up pmlog context " << logContextName << std::endl;
        exit(EXIT_FAILURE);
    }
    PM_LOG_INFO(MSGID_INIT, INIT_KVCOUNT, "Registered for PmLog");
    if (SIG_ERR == signal(SIGTERM, signalHandler))
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "Failed to set SIGTERM handler!");
        return -1;
    }

    if (SIG_ERR == signal(SIGINT, signalHandler))
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "Failed to set SIGINT handler!");
        return -1;
    }
    mainLoop = g_main_loop_new(NULL, FALSE);

    if(NULL == mainLoop)
    {
        return -1;
    }

    if(false == audioServiceRegister("com.webos.service.audiofocusmanager", mainLoop, &gServiceHandle))
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "Failed to set Register service!");
        return 0;
    }
    audioFocusManager = AudioFocusManager::getInstance();
    if (!audioFocusManager)
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "Couldn't get the Audiofocusmanager instance!");
        return 0;
    }
    if(audioFocusManager->init(mainLoop) == false)
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "getInstance failed: Failed to initialize the object!");
        g_main_loop_unref(mainLoop);
        delete audioFocusManager;
        audioFocusManager = NULL;
        return -1;
    }

    g_main_loop_run(mainLoop);
    g_main_loop_unref(mainLoop);

    LSError lserror;
    LSErrorInit(&lserror);

    if(!LSUnregister(GetLSService(), &lserror))
    {
        LSErrorFree(&lserror);
    }

    PM_LOG_INFO(MSGID_STUTDOWN, INIT_KVCOUNT, "Service is unregistered");
    return 0;
}
