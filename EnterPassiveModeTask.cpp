/**
 * @desc:   显示当前目录任务类，EnterPassiveModeTask.cpp
 * @author: zhangxf
 * @date:   2025.05.08
 */

#include "EnterPassiveModeTask.h"

#include "FTPClient.h"


void EnterPassiveModeTask::doTask()
{
    FTPClient::getInstance().pasv();
}