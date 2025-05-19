/**
 * @desc:   主动模式下与FTPServer协商ip地址与端口号人物，PortTask.h
 * @author: zhangxf
 * @date:   2025.05.19
 */

#ifndef PORT_TASK_H_
#define PORT_TASK_H_

#include "Task.h"

class PortTask : public Task
{
public:
    PortTask() = default;
    ~PortTask() = default;

public:
    void doTask() override;

};

#endif //!PORT_TASK_H_