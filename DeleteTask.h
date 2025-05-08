/**
 * @desc:   删除文件或者目录任务，DeleteTask.h
 * @author: zhangxf
 * @date:   2025.05.08
 */

#ifndef DELETE_TASK_H_
#define DELETE_TASK_H_

#include <string>

#include "Task.h"

class DeleteTask : public Task
{
public:
    DeleteTask(const std::string& targetFileOrDir);
    ~DeleteTask() = default;

public:
    void doTask() override;

private:
    std::string   m_targetFileOrDir;

};

#endif //!DELETE_TASK_H_