/**
 * @desc:   数据处理类，数据处理类有自己的线程池，Processor.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#include <cstdint>

#ifndef PROCESSOR_H_
#define PROCESSOR_H_

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <vector>

#include "Task.h"

class Processor final
{

public:
    static Processor& getInstance();

public:
    void init();
    void uninit();

    void addSendTask(Task* pTask);
    void addRecvTask(Task* pTask);


private:
    Processor() = default;
    ~Processor() = default;

    //TODO: 

    void sendThreadFunc();
    void recvThreadFunc();


private:
    std::unique_ptr<std::thread>              m_spSendThread;
    std::unique_ptr<std::thread>              m_spRecvThread;

    //标记任务处理线程是否退出的标记，为true表示不需要退出，为false表示需要退出
    std::atomic<bool>                         m_running;

    std::list<Task*>                          m_sendTasks;
    std::list<Task*>                          m_recvTasks;

    std::mutex                                m_sendMutex;
    std::mutex                                m_recvMutex;

    std::condition_variable                   m_sendCV;
    std::condition_variable                   m_recvCV;
};


#endif //!PROCESSOR_H_