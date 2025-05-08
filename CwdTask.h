/**
 * @desc:   进入选定目录任务类，CwdTask.h
 * @author: zhangxf
 * @date:   2025.05.08
 */

#ifndef CWD_TASK_H_
#define CWD_TASK_H_

#include <string>

#include "Task.h"

class CwdTask : public Task
{
public:
    CwdTask(const std::string& targetDir);
    ~CwdTask() = default;

public:
    void doTask() override;

private:
    std::string   m_targetDir;

};

#endif //!CWD_TASK_H_