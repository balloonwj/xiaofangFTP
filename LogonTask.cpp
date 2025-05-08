/**
 * @desc:   登录任务类，LogonTask.cpp
 * @author: zhangxf
 * @date:   2025.04.10
 */

#include "LogonTask.h"

#include "FTPClient.h"


void LogonTask::doTask()
{
    FTPClient::getInstance().logon();
}