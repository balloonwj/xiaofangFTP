/**
 * @desc:   建立网络连接的任务类，ConnectTask.h
 * @author: zhangxf
 * @date:   2025.04.17
 */

#include "ConnectTask.h"

#include <codecvt>
#include <locale>

#include "FTPClient.h"

ConnectTask::ConnectTask(const std::wstring& ip, uint16_t port) :
    m_ip(ip), m_port(port)
{

}

void ConnectTask::doTask()
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::string ip = converter.to_bytes(m_ip);
    //std::string userName = converter.to_bytes(m_userName);
    //std::string password = converter.to_bytes(m_password);

    //if (!FTPClient::getInstance().connect(ip, m_port))
    //{
    //    //TODO: 连接不成功，报告错误，并重试
    //    return;
    //}

    //FTPClient::getInstance().recvBuf();

    FTPClient::getInstance().connectWithResponse(ip, m_port);
}
