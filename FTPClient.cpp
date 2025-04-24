/**
 * @desc:   网络接口API，FTPClient.cpp
 * @author: zhangxf
 * @date:   2025.04.17
 */

#include "FTPClient.h"

#include <chrono>
#include <functional>

#include "CharChecker.h"

#include "./xiaofangLog/AsyncLog.h"

#pragma comment(lib, "Ws2_32.lib")

#define MAX_RESPONSE_LENGTH 256

enum FTP_STATUS_CODE
{
    SERVICE_READY_FOR_NEW_USER = 220,
    USER_NAME_OKAY_NEED_PASSWORD = 331,
    USER_LOGGED_IN = 230,
    PATHNAME_CREATED = 257, //显示当前路径成功
    ENTERING_PASSIVE_MODE = 227 //进入被动模式
};

FTPClient& FTPClient::getInstance()
{
    static FTPClient ftpServer;
    return ftpServer;
}

void FTPClient::startNetworkThread()
{
    if (m_running)
        return;

    m_running = true;

    m_spNetworkThread = std::make_unique<std::thread>(std::bind(&FTPClient::networkThreadFunc, this));
}

void FTPClient::stopNetworkThread()
{
    m_running = false;

    m_spNetworkThread->join();
}

bool FTPClient::connect(int timeout /*= 3*/)
{
    m_hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_hSocket == INVALID_SOCKET)
        return false;

    long tmSend = 3 * 1000L;
    long tmRecv = 3 * 1000L;
    long noDelay = 1;
    setsockopt(m_hSocket, IPPROTO_TCP, TCP_NODELAY, (LPSTR)&noDelay, sizeof(long));
    setsockopt(m_hSocket, SOL_SOCKET, SO_SNDTIMEO, (LPSTR)&tmSend, sizeof(long));
    setsockopt(m_hSocket, SOL_SOCKET, SO_RCVTIMEO, (LPSTR)&tmRecv, sizeof(long));

    //将socket设置成非阻塞的
    unsigned long on = 1;
    if (::ioctlsocket(m_hSocket, FIONBIO, &on) == SOCKET_ERROR)
        return false;

    struct sockaddr_in addrSrv = { 0 };
    struct hostent* pHostent = NULL;
    unsigned int addr = 0;

    if ((addrSrv.sin_addr.s_addr = inet_addr(m_ip.c_str())) == INADDR_NONE)
    {
        pHostent = ::gethostbyname(m_ip.c_str());
        if (!pHostent)
        {
            //LOG_ERROR("Could not connect server:%s, port:%d.", m_strServer.c_str(), port);
            return false;
        }
        else
            addrSrv.sin_addr.s_addr = *((unsigned long*)pHostent->h_addr);
    }

    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons((u_short)m_port);
    int ret = ::connect(m_hSocket, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
    if (ret == 0)
    {
        //LOG_INFO("Connect to server:%s, port:%d successfully.", m_strServer.c_str(), m_nPort);
        m_bConnected = true;
        return true;
    }
    else if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
    {
        //LOG_ERROR("Could not connect to server:%s, port:%d.", m_strServer.c_str(), m_nPort);
        return false;
    }

    fd_set writeset;
    FD_ZERO(&writeset);
    FD_SET(m_hSocket, &writeset);
    struct timeval tv = { timeout, 0 };
    if (::select(m_hSocket + 1, NULL, &writeset, NULL, &tv) != 1)
    {
        //LOG_ERROR("Could not connect to server:%s, port:%d.", m_strServer.c_str(), m_nPort);
        return false;
    }

    m_bConnected = true;

    return true;
}

bool FTPClient::sendBuf()
{
    int n;

    while (true)
    {
        n = ::send(m_hSocket, m_sendBuf.c_str(), m_sendBuf.size(), 0);
        if (n == 0)
        {
            //对端关闭了连接
            return false;
        }
        else if (n < 0)
        {
            if (::WSAGetLastError() == WSAEWOULDBLOCK)
            {
                //当前TCP窗口太小，数据暂时发不出去
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            else
            {
                //发送数据出错了
                return false;
            }
        }
        else
        {
            if (n == static_cast<int>(m_sendBuf.size()))
            {
                LOGI("==>%s", m_sendBuf.c_str());
                m_sendBuf.clear();

                //数据都发完了
                return true;
            }
            else
            {
                //只发送了部分数据
                m_sendBuf.erase(0, n);
            }
        }
    }
}

bool FTPClient::recvBuf()
{
    while (true)
    {
        char buf[64] = { 0 };
        int bytesRecv = recv(m_hSocket, buf, 64, 0);
        if (bytesRecv == 0)
        {
            return false;
        }
        else if (bytesRecv < 0)
        {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
            {
                //当前没有数据
                break;
            }
            else
            {
                return false;
            }
        }

        //bytesRecv > 0
        m_recvBuf.append(buf, bytesRecv);
    }


    return true;
}

DecodePackageResult FTPClient::decodePackage()
{
    return m_protocolParser.parseFTPResponse(m_recvBuf, m_responseLines);
}

bool FTPClient::parseState()
{
    for (auto& iter : m_responseLines)
    {
        if (iter.isEnd)
        {
            switch (iter.statusCode)
            {
            case SERVICE_READY_FOR_NEW_USER:
                m_clientState = FTPClientState::WELCOMEMSGRECEIVED;
                return true;

            case USER_NAME_OKAY_NEED_PASSWORD:
                m_clientState = FTPClientState::USERNAMEOKAYNEEDPASSWORD;
                return true;

            case USER_LOGGED_IN:
                m_clientState = FTPClientState::LOGON;
                return true;

            case PATHNAME_CREATED:
                m_clientState = FTPClientState::LOGON;
                return true;

            case ENTERING_PASSIVE_MODE:
                m_clientState = FTPClientState::LOGON;

                //TODO: 还需要进一步解析数据中的ip地址和端口号
                return true;

            default:
                break;
            }
        }
    }

    return false;
}

void FTPClient::setServerInfo(const std::string& ip, uint16_t port,
    const std::string& userName, const std::string& password,
    bool isPassiveMode)
{
    m_ip = ip;
    m_port = port;

    m_userName = userName;
    m_password = password;

    m_isPassiveMode = isPassiveMode;
}

FTPClient::FTPClient()
{
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    ::WSAStartup(wVersionRequested, &wsaData);
}

FTPClient::~FTPClient()
{
    ::WSACleanup();
}

void FTPClient::networkThreadFunc()
{
    while (m_running)
    {
        //建立socket连接
        if (!m_bConnected)
        {
            if (connect())
            {
                LOGI("Connected to %s:%d", m_ip.c_str(), m_port);

                m_clientState = FTPClientState::CONNECTED;
            }
            else
            {
                //TODO：反馈给UI层，网络连接不上，稍后重试

                std::this_thread::sleep_for(std::chrono::milliseconds(3000));

                continue;
            }
        }


        //判断是否有数据需要接收，有则收之，并解包，解包成功之后，反馈给UI层
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(m_hSocket, &readset);
        //struct timeval tv = { 0, 100000 };
        struct timeval tv = { 1, 0 };
        int ret = ::select(m_hSocket + 1, &readset, NULL, NULL, &tv);
        if (ret == 0)
        {
            //select超时
            //LOGI("No data...");
        }
        else if (ret == 1)
        {
            //有读事件
            //收包
            if (!recvBuf())
            {
                //关闭连接，等待重连
                continue;
            }

            //解包
            DecodePackageResult result = decodePackage();
            if (result == DecodePackageResult::FAULT)
            {
                //TODO: 关闭连接，并重试
                continue;
            }
            else if (result == DecodePackageResult::WANTMOREDATA)
            {
                continue;
            }
            else
            {
                //TODO: 成功拿到了包，交给界面展示即可
                for (const auto& line : m_responseLines)
                {
                    LOGI("%d %s", line.statusCode, line.statusText.c_str());
                }

                if (!parseState())
                {
                    //TODO: 关闭连接，并重试
                    continue;
                }

                m_responseLines.clear();
            }

        }
        else
        {
            //TODO: select函数调用出错了，关闭socket进行重连
            continue;
        }


        //查看是否有数据需要发送，有则发送
        switch (m_clientState)
        {
        case FTPClientState::WELCOMEMSGRECEIVED:
            //走登录逻辑，发用户名
            m_sendBuf.append("USER " + m_userName + "\r\n");
            break;

        case FTPClientState::USERNAMEOKAYNEEDPASSWORD:
            //走登录逻辑，发密码
            m_sendBuf.append("PASS " + m_password + "\r\n");
            break;

        case FTPClientState::LOGON:
            //登录成功
            if (m_isPassiveMode)
            {
                //修改成被动模式
                m_sendBuf.append("PASV\r\n");
            }
            break;

        default:
            //
            continue;
        }

        if (!sendBuf())
        {
            //关闭连接重试
        }
    }
}