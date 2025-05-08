/**
 * @desc:   显示当前目录任务类，PwdTask.h
 * @author: zhangxf
 * @date:   2025.05.08
 */

#ifndef PWD_TASK_H_
#define PWD_TASK_H_

#include "Task.h"

class PwdTask : public Task
{
public:
    PwdTask() = default;
    ~PwdTask() = default;

public:
    void doTask() override;

};

#endif //!PWD_TASK_H_