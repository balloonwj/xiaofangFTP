/**
 * @desc:   数据处理类，数据处理类有自己的线程池，Processor.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#include "Processor.h"

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "./xiaofangLog/AsyncLog.h"

 //TODO: 可以使用Windows操作系统的线程池API

Processor& Processor::getInstance()
{
    static Processor p;
    return p;
}

void Processor::init()
{
    m_running = true;

    m_spSendThread = std::make_unique<std::thread>(std::bind(&Processor::sendThreadFunc, this));

    m_spRecvThread = std::make_unique<std::thread>(std::bind(&Processor::recvThreadFunc, this));
}

void Processor::uninit()
{
    m_running = false;

    m_sendCV.notify_one();
    m_recvCV.notify_one();

    m_spSendThread->join();
    m_spRecvThread->join();

    LOGI("Processor::uninit......");
}

void Processor::addSendTask(Task* pTask)
{
    std::unique_lock<std::mutex> guard(m_sendMutex);
    m_sendTasks.push_back(pTask);

    m_sendCV.notify_one();
}

void Processor::addRecvTask(Task* pTask)
{
    std::unique_lock<std::mutex> guard(m_recvMutex);
    m_recvTasks.push_back(pTask);

    m_recvCV.notify_one();
}

void Processor::sendThreadFunc()
{
    LOGI("sendThreadFunc start.");

    while (m_running)
    {
        std::unique_lock<std::mutex> guard(m_sendMutex);
        while (m_sendTasks.empty())
        {
            if (!m_running)
            {
                return;
            }

            //如果获得了互斥锁，但是条件不合适的话，pthread_cond_wait会释放锁，不往下执行。
            //当发生变化后，条件合适，pthread_cond_wait将直接获得锁。
            m_sendCV.wait(guard);
        }

        Task* pTask = m_sendTasks.front();
        m_sendTasks.pop_front();

        if (pTask == NULL)
            continue;

        pTask->doTask();
        delete pTask;
        pTask = NULL;
    }
}

void Processor::recvThreadFunc()
{
    LOGI("recvThreadFunc start.");

    while (m_running)
    {
        std::unique_lock<std::mutex> guard(m_recvMutex);
        while (m_recvTasks.empty())
        {
            if (!m_running)
            {
                return;
            }

            //如果获得了互斥锁，但是条件不合适的话，pthread_cond_wait会释放锁，不往下执行。
            //当发生变化后，条件合适，pthread_cond_wait将直接获得锁。
            m_recvCV.wait(guard);
        }

        Task* pTask = m_recvTasks.front();
        m_recvTasks.pop_front();

        if (pTask == NULL)
            continue;

        pTask->doTask();
        delete pTask;
        pTask = NULL;
    }


}