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

#include "common.h"
#include <lunaservice.h>
#include "log.h"


void CLSError::Print(const char * where, int line, GLogLevelFlags logLevel)
{
    if (LSErrorIsSet(this))
    {
        PM_LOG_ERROR(MSGID_CORE, INIT_KVCOUNT, "%s(%d): Luna Service Error #%d \"%s\",  \
                                      \nin %s line #%d.", where, line,
                                      this->error_code, this->message,
                                      this->file, this->line);
        LSErrorFree(this);
    }
}

void LSMessageResponse(LSHandle* handle, LSMessage * message, const char* reply, RESPONSE_TYPE eType, bool isReferenced)
{
    CLSError lserror;
    if (message)
    {
        PM_LOG_INFO(MSGID_PARSE_JSON, INIT_KVCOUNT,"AudioD response with params:'%s'", reply);
        if (eLSReply == eType)
        {
            if (handle)
            {
                if (!LSMessageReply(handle, message, reply, &lserror))
                    lserror.Print(__FUNCTION__, __LINE__);
            }
        }
        else if (eLSRespond == eType)
        {
            if (!LSMessageRespond(message, reply, &lserror))
                lserror.Print(__FUNCTION__, __LINE__);
        }
        else
            PM_LOG_ERROR(MSGID_INVALID_INPUT, INIT_KVCOUNT, "Unknown LsReply type: %d",eType);
        if (isReferenced)
           LSMessageUnref(message);
    }
    else
        PM_LOG_ERROR(MSGID_DATA_NULL, INIT_KVCOUNT, "LSMessage data is not available to Reply/Respond");
}
