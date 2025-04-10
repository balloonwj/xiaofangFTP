/**
 * @desc:   任务类基类，Task.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#ifndef TASK_H_
#define TASK_H_

class Task
{
public:
    Task() = default;
    ~Task() = default;

public:
    virtual void doTask() = 0;
};

#endif //!TASK_H_
