/**
 * @desc:   进入选定目录任务类，CwdTask.cpp
 * @author: zhangxf
 * @date:   2025.05.08
 */

#include "CwdTask.h"

#include "FTPClient.h"

CwdTask::CwdTask(const std::string& targetDir) : m_targetDir(targetDir)
{

}

void CwdTask::doTask()
{
    FTPClient::getInstance().cwd(m_targetDir);
}