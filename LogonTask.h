/**
 * @desc:   登录任务类，LogonTask.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#ifndef LOGON_TASK_H_
#define LOGON_TASK_H_

class LogonTask
{
public:
    LogonTask();
    ~LogonTask();

public:
    void doTask() override;

};

#endif //!LOGON_TASK_H_