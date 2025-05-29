/**
 * @desc:   下载文件任务，DownloadTask.cpp
 * @author: zhangxf
 * @date:   2025.05.29
 */

#include "DownloadTask.h"

#include "FTPClient.h"

DownloadTask::DownloadTask(const std::string& localFilePath, const std::string& serverFileName)
    : m_localFilePath(localFilePath), m_serverFileName(serverFileName)
{

}


void DownloadTask::doTask()
{
    FTPClient::getInstance().download(m_localFilePath, m_serverFileName);
}