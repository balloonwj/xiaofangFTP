/**
 * @desc:   主动模式下与FTPServer协商ip地址与端口号人物，PortTask.cpp
 * @author: zhangxf
 * @date:   2025.05.19
 */

#include "PortTask.h"

#include "FTPClient.h"


void PortTask::doTask()
{
    FTPClient::getInstance().port();
}