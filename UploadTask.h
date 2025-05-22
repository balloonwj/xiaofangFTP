/**
 * @desc:   上传文件任务，UploadTask.h
 * @author: zhangxf
 * @date:   2025.05.22
 */

#ifndef UPLOAD_TASK_H_
#define UPLOAD_TASK_H_

#include "Task.h"

#include <string>

class UploadTask : public Task
{
public:
    UploadTask(const std::string& localFilePath, const std::string& serverFileName);
    ~UploadTask() = default;

public:
    void doTask() override;

private:
    std::string     m_localFilePath;
    std::string     m_serverFileName;
};

#endif //!UPLOAD_TASK_H_