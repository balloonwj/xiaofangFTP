/**
 * @desc:   登录任务类，LogonTask.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#ifndef LOGON_TASK_H_
#define LOGON_TASK_H_

#include "Task.h"

class LogonTask : public Task
{
public:
    LogonTask() = default;
    ~LogonTask() = default;

public:
    void doTask() override;

};

#endif //!LOGON_TASK_H_