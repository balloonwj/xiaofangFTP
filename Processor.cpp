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

 //TODO: 可以使用Windows操作系统的线程池API

bool Processor::init(int8_t threadCount/* = 4*/)
{
    //TODO: 对threadCount进行校验

    for (int i = 0; i < threadCount; i++)
    {
        //https://cppguide.cn/pages/essentialsofcppserverprogrammingch03-03/

        auto spThread = std::make_shared<std::thread>(
            std::bind(&Processor::threadfunc, this));

        m_threads.push_back(std::move(spThread));
    }


}

bool Processor::uninit()
{
    size_t threadCount = m_threads.size();
    for (size_t i = 0; i < threadCount; i++)
    {
        m_threads[i]->join();
    }
}

void Processor::threadfunc()
{
    while (退出标志)
    {
        std::unique_lock<std::mutex> guard(m_mutex);
        while (m_tasks.empty())
        {
            //如果获得了互斥锁，但是条件不合适的话，pthread_cond_wait会释放锁，不往下执行。
            //当发生变化后，条件合适，pthread_cond_wait将直接获得锁。
            m_cv.wait(guard);
        }

        Task* pTask = m_tasks.front();
        tasks.pop_front();

        if (pTask == NULL)
            continue;

        pTask->doTask();
        delete pTask;
        pTask = NULL;
    }


}