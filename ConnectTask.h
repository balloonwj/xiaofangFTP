/**
 * @desc:   建立网络连接的任务类，ConnectTask.h
 * @author: zhangxf
 * @date:   2025.04.17
 */

#pragma once

#include "Task.h"

#include <string>

#include <cstdint>

class ConnectTask : public Task
{
public:
    ConnectTask(const std::wstring& ip, uint16_t port, const std::wstring& userName, std::wstring& password);
    ~ConnectTask() = default;

public:
    void doTask() override;


private:
    std::wstring     m_ip;
    uint16_t         m_port;
    std::wstring     m_userName;
    std::wstring     m_password;
};
