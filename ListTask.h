/**
 * @desc:   拉取当前目录列表任务类，ListTask.h
 * @author: zhangxf
 * @date:   2025.05.19
 */

#ifndef LIST_TASK_H_
#define LIST_TASK_H_

#include "Task.h"

class ListTask : public Task
{
public:
    ListTask() = default;
    ~ListTask() = default;

public:
    void doTask() override;

};

#endif //!LIST_TASK_H_