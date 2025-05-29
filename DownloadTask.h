/**
 * @desc:   下载文件任务，DownloadTask.h
 * @author: zhangxf
 * @date:   2025.05.29
 */

#ifndef DOWNLOAD_TASK_H_
#define DOWNLOAD_TASK_H_

#include "Task.h"

#include <string>

class DownloadTask : public Task
{
public:
    DownloadTask(const std::string& localFilePath, const std::string& serverFileName);
    ~DownloadTask() = default;

public:
    void doTask() override;

private:
    std::string     m_localFilePath;
    std::string     m_serverFileName;
};

#endif //!DOWNLOAD_TASK_H_