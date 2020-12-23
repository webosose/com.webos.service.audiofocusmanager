/* @@@LICENSE
 *
 * Copyright (c) 2020 LG Electronics, Inc.
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

#include "log.h"

static const char* const logContextName = "AudioFocusManager";
static const char* const logPrefix= "[AF]";

static GMainLoop *mainLoop = NULL;
AudioFocusManager *AudioFocusManagerService = NULL;

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
    PM_LOG_INFO(MSGID_STUTDOWN, INIT_KVCOUNT, "deleted audioController object");
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

    AudioFocusManagerService = AudioFocusManager::getInstance();

    if(NULL == AudioFocusManagerService)
    {
        PM_LOG_ERROR(MSGID_INIT, INIT_KVCOUNT, "getInstance failed: Failed to initialize the object!");
        g_main_loop_unref(mainLoop);
        return -1;
    }

    if(AudioFocusManagerService->init(mainLoop) == false)
    {
        g_main_loop_unref(mainLoop);
        delete AudioFocusManagerService;
        AudioFocusManagerService = NULL;
        return -1;
    }

    g_main_loop_run(mainLoop);
    g_main_loop_unref(mainLoop);

    LSError lserror;
    LSErrorInit(&lserror);

    if(!LSUnregister(AudioFocusManager::mServiceHandle, &lserror))
    {
        LSErrorFree(&lserror);
    }

    PM_LOG_INFO(MSGID_STUTDOWN, INIT_KVCOUNT, "Service is unregistered");
    return 0;
}
