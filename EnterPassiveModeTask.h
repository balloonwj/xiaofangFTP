/**
 * @desc:   进入被动模式任务类，EnterPassiveModeTask.h
 * @author: zhangxf
 * @date:   2025.05.08
 */

#ifndef ENTER_PASSIVE_MODE_TASK_H_
#define ENTER_PASSIVE_MODE_TASK_H_

#include "Task.h"

class EnterPassiveModeTask : public Task
{
public:
    EnterPassiveModeTask() = default;
    ~EnterPassiveModeTask() = default;

public:
    void doTask() override;

};

#endif //!ENTER_PASSIVE_MODE_TASK_H_