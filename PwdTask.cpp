/**
 * @desc:   显示当前目录任务类，PwdTask.cpp
 * @author: zhangxf
 * @date:   2025.05.08
 */

#include "PwdTask.h"

#include "FTPClient.h"


void PwdTask::doTask()
{
    FTPClient::getInstance().pwd();
}