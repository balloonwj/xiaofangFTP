/**
 * @desc:   拉取当前目录列表任务类，ListTask.cpp
 * @author: zhangxf
 * @date:   2025.05.19
 */

#include "ListTask.h"

#include "FTPClient.h"


void ListTask::doTask()
{
    FTPClient::getInstance().list();
}