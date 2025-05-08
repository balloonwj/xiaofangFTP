/**
 * @desc:   删除文件或者目录任务，DeleteTask.cpp
 * @author: zhangxf
 * @date:   2025.05.08
 */

#include "DeleteTask.h"

#include "FTPClient.h"

DeleteTask::DeleteTask(const std::string& targetFileOrDir) : m_targetFileOrDir(targetFileOrDir)
{

}

void DeleteTask::doTask()
{
    FTPClient::getInstance().del(m_targetFileOrDir);
}