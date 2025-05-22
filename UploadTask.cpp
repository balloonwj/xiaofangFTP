/**
 * @desc:   上传文件任务，UploadTask.cpp
 * @author: zhangxf
 * @date:   2025.05.19
 */

#include "UploadTask.h"

#include "FTPClient.h"

UploadTask::UploadTask(const std::string& localFilePath, const std::string& serverFileName)
    : m_localFilePath(localFilePath), m_serverFileName(serverFileName)
{

}


void UploadTask::doTask()
{
    FTPClient::getInstance().upload(m_localFilePath, m_serverFileName);
}