/**
 * @desc:   网络接口API，FTPServer.cpp
 * @author: zhangxf
 * @date:   2025.04.17
 */

#include "FTPServer.h"

#include "CharChecker.h"

#pragma comment(lib, "Ws2_32.lib")

#define MAX_RESPONSE_LENGTH 256

FTPServer& FTPServer::getInstance()
{
    static FTPServer ftpServer;
    return ftpServer;
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

    std::string recvBuf;

    while (true)
    {
        char buf[64] = { 0 };
        int bytesRecv = recv(m_hSocket, buf, 1024, 0);
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
        recvBuf.append(buf, bytesRecv);
    }


    //解包
    if (!decodePackage(recvBuf))
    {
        return false;
    }

    return true;
}

DecodePackageResult FTPServer::decodePackage(std::string& recvBuf)
{
    int i = 0;
    char ch;
    while (true)
    {
        if (recvBuf.length() <= 4)
        {
            return DecodePackageResult::WANTMOREDATA;
        }

        if (CharChecker::isDigit(recvBuf[i]) &&
            CharChecker::isDigit(recvBuf[i + 1]) &&
            CharChecker::isDigit(recvBuf[i + 2]))
        {
            return DecodePackageResult::FAULT;
        }

        if (recvBuf[i + 3] == ' ')
        {
            //单行的响应
            i++;
            while (i < recvBuf.length())
            {
                if (recvBuf[i] == '\n')
                {
                    //找到了一个包的结尾
                    std::string aPackage = recvBuf.substr(0, i + 1);
                    recvBuf.erase(0, i + 1);
                    i = 0;
                    break;
                }
                else if (i >= MAX_RESPONSE_LENGTH)
                {
                    return DecodePackageResult::FAULT;
                }
            }

            //继续解下一个包
            continue;
        }
        else if (recvBuf[i + 3] == '-')
        {
            //多行的响应
            i++;
            while (i < recvBuf.length())
            {
                if (recvBuf[i] == '\n')
                {
                    if (i + 5 < recvBuf.length())
                    {
                        if (CharChecker::isDigit(recvBuf[i + 1]) &&
                            CharChecker::isDigit(recvBuf[i + 2]) &&
                            CharChecker::isDigit(recvBuf[i + 3]))
                        {
                            if (recvBuf[i + 4] == ' ')
                            {
                                //找到多行响应的最后一行
                                i = i + 5;
                                while (i < recvBuf.length())
                                {
                                    if (recvBuf[i] == '\n')
                                    {
                                        //找到了一个包的结尾
                                        std::string aPackage = recvBuf.substr(0, i + 1);
                                        recvBuf.erase(0, i + 1);
                                        i = 0;
                                        break;
                                    }
                                    else if (i >= MAX_RESPONSE_LENGTH)
                                    {
                                        return DecodePackageResult::FAULT;
                                    }
                                }
                            }
                        }
                        else
                        {
                            //包不完整
                            return DecodePackageResult::WANTMOREDATA;
                        }
                    }
                    else
                    {
                        //包不完整
                        return DecodePackageResult::WANTMOREDATA;
                    }
                    //找到了一个包的结尾
                    std::string aPackage = recvBuf.substr(0, i + 1);
                    recvBuf.erase(0, i + 1);
                    i = 0;
                    break;
                }
                else if (i >= MAX_RESPONSE_LENGTH)
                {
                    return DecodePackageResult::FAULT;
                }
            }
        }
        else
        {
            return DecodePackageResult::FAULT;
        }



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