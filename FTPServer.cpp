/**
 * @desc:   网络接口API，FTPServer.cpp
 * @author: zhangxf
 * @date:   2025.04.17
 */

#include "FTPServer.h"

#include <functional>

#include "CharChecker.h"

#pragma comment(lib, "Ws2_32.lib")

#define MAX_RESPONSE_LENGTH 256

FTPServer& FTPServer::getInstance()
{
    static FTPServer ftpServer;
    return ftpServer;
}

void FTPServer::startNetworkThread()
{
    m_running = true;

    m_spNetworkThread = std::make_unique<std::thread>(std::bind(&FTPServer::networkThreadFunc, this));
}

void FTPServer::stopNetworkThread()
{
    m_running = false;

    m_spNetworkThread->join();
}

bool FTPServer::connect(const std::string& ip, uint16_t port, int timeout /*= 3*/)
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

    if ((addrSrv.sin_addr.s_addr = inet_addr(ip.c_str())) == INADDR_NONE)
    {
        pHostent = ::gethostbyname(ip.c_str());
        if (!pHostent)
        {
            //LOG_ERROR("Could not connect server:%s, port:%d.", m_strServer.c_str(), port);
            return false;
        }
        else
            addrSrv.sin_addr.s_addr = *((unsigned long*)pHostent->h_addr);
    }

    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons((u_short)port);
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

bool FTPServer::recvBuf()
{
    if (!m_bConnected)
        return false;

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


    std::vector<ResponseLine> responseLines;

    DecodePackageResult result = m_protocolParser.parseFTPResponse(m_recvBuf, responseLines);
    if (result == DecodePackageResult::FAULT)
    {
        //TODO: 关闭连接，并重试

        return false;
    }
    else if (result == DecodePackageResult::WANTMOREDATA)
    {
        return true;
    }
    else
    {
        //TODO: 成功拿到了包，交给界面展示即可

        return true;
    }

}

FTPServer::FTPServer()
{
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    ::WSAStartup(wVersionRequested, &wsaData);
}

FTPServer::~FTPServer()
{
    ::WSACleanup();
}

void FTPServer::networkThreadFunc()
{
    while (m_running)
    {
        //建立socket连接

        //查看是否有数据需要发送，有则发送

        //判断是否有数据需要接收，有则收之，并解包，解包成功之后，反馈给UI层
    }
}