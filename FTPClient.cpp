/**
 * @desc:   网络接口API，FTPClient.cpp
 * @author: zhangxf
 * @date:   2025.04.17
 */

#include "FTPClient.h"

#include <chrono>
#include <functional>

#include <ws2tcpip.h>   //for getnameinfo

#include "CharChecker.h"
#include "StringUtil.h"

#include "./xiaofangLog/AsyncLog.h"

#pragma comment(lib, "Ws2_32.lib")

#define MAX_RESPONSE_LENGTH 256

enum FTP_STATUS_CODE
{
    FILE_STATUS_OKAY_ABOUT_TO_OPEN_DATA_CONNECTION = 150,   //数据传输端口已经ready，准备开始传输数据   
    COMMAND_OKAY = 200,
    SERVICE_READY_FOR_NEW_USER = 220,
    USER_NAME_OKAY_NEED_PASSWORD = 331,
    USER_LOGGED_IN = 230,
    PATHNAME_CREATED = 257, //显示当前路径成功
    ENTERING_PASSIVE_MODE = 227, //进入被动模式
    REQUESTED_FILE_ACTION_OKAY_COMPLETED = 250, //进入目录成功
    REQUESTED_ACTION_NOT_TAKEN = 550, //权限不足或者文件不存在
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

    //m_spNetworkThread = std::make_unique<std::thread>(std::bind(&FTPClient::networkThreadFunc, this));
}

void FTPClient::stopNetworkThread()
{
    m_running = false;

    m_spNetworkThread->join();
}

bool FTPClient::connect(int timeout /*= 3*/)
{
    m_hControlSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_hControlSocket == INVALID_SOCKET)
        return false;

    long tmSend = 3 * 1000L;
    long tmRecv = 3 * 1000L;
    long noDelay = 1;
    setsockopt(m_hControlSocket, IPPROTO_TCP, TCP_NODELAY, (LPSTR)&noDelay, sizeof(long));
    setsockopt(m_hControlSocket, SOL_SOCKET, SO_SNDTIMEO, (LPSTR)&tmSend, sizeof(long));
    setsockopt(m_hControlSocket, SOL_SOCKET, SO_RCVTIMEO, (LPSTR)&tmRecv, sizeof(long));

    //将socket设置成非阻塞的
    unsigned long on = 1;
    if (::ioctlsocket(m_hControlSocket, FIONBIO, &on) == SOCKET_ERROR)
    {
        return false;
    }

    struct sockaddr_in addrSrv = { 0 };
    struct hostent* pHostent = NULL;
    unsigned int addr = 0;

    if ((addrSrv.sin_addr.s_addr = inet_addr(m_controlIP.c_str())) == INADDR_NONE)
    {
        pHostent = ::gethostbyname(m_controlIP.c_str());
        if (!pHostent)
        {
            //LOG_ERROR("Could not connect server:%s, port:%d.", m_strServer.c_str(), port);
            return false;
        }
        else
            addrSrv.sin_addr.s_addr = *((unsigned long*)pHostent->h_addr);
    }

    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons((u_short)m_controlPort);
    int ret = ::connect(m_hControlSocket, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
    if (ret == 0)
    {
        //LOG_INFO("Connect to server:%s, port:%d successfully.", m_strServer.c_str(), m_nPort);
        m_bControlChannelConnected = true;
        return true;
    }
    else if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
    {
        //LOG_ERROR("Could not connect to server:%s, port:%d.", m_strServer.c_str(), m_nPort);
        return false;
    }

    fd_set writeset;
    FD_ZERO(&writeset);
    FD_SET(m_hControlSocket, &writeset);
    struct timeval tv = { timeout, 0 };
    if (::select(m_hControlSocket + 1, NULL, &writeset, NULL, &tv) != 1)
    {
        //LOG_ERROR("Could not connect to server:%s, port:%d.", m_strServer.c_str(), m_nPort);
        return false;
    }

    m_bControlChannelConnected = true;

    return true;
}

bool FTPClient::connectWithResponse()
{
    if (!connect())
    {
        close();
        return false;
    }

    if (!checkReadable())
    {
        close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!recvBuf(responseLines))
    {
        close();
        return false;
    }

    for (const auto& line : responseLines)
    {
        if (line.isEnd && line.statusCode == SERVICE_READY_FOR_NEW_USER)
            return true;
    }

    return false;
}

bool FTPClient::logon(/*const char* username, const char* password*/)
{
    if (!m_bControlChannelConnected)
        return false;

    //发送用户名
    std::string req("USER ");
    req.append(m_userName);
    req.append("\r\n");

    if (!sendBuf(req))
    {
        close();
        return false;
    }

    if (!checkReadable())
    {
        close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!recvBuf(responseLines))
    {
        close();
        return false;
    }

    bool ok = false;
    for (const auto& line : responseLines)
    {
        if (line.isEnd && line.statusCode == USER_NAME_OKAY_NEED_PASSWORD)
            ok = true;
    }

    if (!ok)
    {
        close();
        return false;
    }


    //发送密码
    req = "PASS ";
    req.append(m_password);
    req.append("\r\n");

    if (!sendBuf(req))
    {
        close();
        return false;
    }

    if (!checkReadable())
    {
        close();
        return false;
    }

    responseLines.clear();
    if (!recvBuf(responseLines))
    {
        close();
        return false;
    }

    for (const auto& line : responseLines)
    {
        if (line.isEnd && line.statusCode == USER_LOGGED_IN)
            return true;
    }

    return false;
}

std::string FTPClient::pwd()
{
    if (!m_bControlChannelConnected)
        return "";

    //发送PWD命令
    std::string req("PWD\r\n");

    if (!sendBuf(req))
    {
        close();
        return "";
    }

    if (!checkReadable())
    {
        close();
        return "";
    }

    std::vector<ResponseLine> responseLines;
    if (!recvBuf(responseLines))
    {
        close();
        return "";
    }

    for (const auto& line : responseLines)
    {
        if (line.isEnd && line.statusCode == PATHNAME_CREATED)
        {
            //TODO: 返回的目录是："/" is current directory，需要进一步解析
            return line.statusText;
        }
    }

    return "";
}

bool FTPClient::pasv()
{
    if (!m_bControlChannelConnected)
        return false;

    //发送PASV命令
    std::string req("PASV\r\n");

    if (!sendBuf(req))
    {
        close();
        return false;
    }

    if (!checkReadable())
    {
        close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!recvBuf(responseLines))
    {
        close();
        return false;
    }

    for (const auto& line : responseLines)
    {
        if (line.isEnd && line.statusCode == ENTERING_PASSIVE_MODE)
        {
            LOGI("Enter passive mode.");

            parseDataIPAndPort(line.statusText);

            return true;
        }
    }

    return false;
}

bool FTPClient::port()
{
    if (!m_bControlChannelConnected)
        return false;

    if (!createDataServer())
        return false;

    if (!getDataServerAddr(m_dataIP, m_dataPort))
        return false;

    //将ip格式从127.0.0.1替换成127,0,0,1
    std::string tmpIPStr(m_dataIP);
    StringUtil::replace(tmpIPStr, ".", ",");

    char reqStr[32] = { 0 };
    sprintf_s(reqStr, "PORT %s,%d,%d\r\n", tmpIPStr.c_str(), m_dataPort / 256, m_dataPort % 256);

    //发送数据格式：
    //PORT 127,0,0,1,211,219
    std::string req(reqStr);

    if (!sendBuf(req))
    {
        close();
        return false;
    }

    if (!checkReadable())
    {
        close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!recvBuf(responseLines))
    {
        close();
        return false;
    }

    for (const auto& line : responseLines)
    {
        if (line.isEnd && line.statusCode == COMMAND_OKAY)
        {
            return true;
        }
    }

    return false;
}

bool FTPClient::cwd(const std::string& targetDir)
{
    if (!m_bControlChannelConnected)
        return false;

    //发送CWD命令
    std::string req("CWD ");
    req.append(targetDir);
    req.append("\r\n");

    if (!sendBuf(req))
    {
        close();
        return false;
    }

    if (!checkReadable())
    {
        close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!recvBuf(responseLines))
    {
        close();
        return false;
    }

    for (const auto& line : responseLines)
    {
        if (line.isEnd && line.statusCode == REQUESTED_FILE_ACTION_OKAY_COMPLETED)
            return true;
    }

    return false;
}

bool FTPClient::del(const std::string& targetFileOrDir)
{
    if (!m_bControlChannelConnected)
        return false;

    //发送DELE命令
    std::string req("DELE ");
    req.append(targetFileOrDir);
    req.append("\r\n");

    if (!sendBuf(req))
    {
        close();
        return false;
    }

    if (!checkReadable())
    {
        close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!recvBuf(responseLines))
    {
        close();
        return false;
    }

    for (const auto& line : responseLines)
    {
        if (line.isEnd) {
            if (line.statusCode == REQUESTED_FILE_ACTION_OKAY_COMPLETED)
                return true;
            else if (line.statusCode == REQUESTED_ACTION_NOT_TAKEN)
            {
                //权限不足或者文件不存在
                //TODO: 失败的情形可以细分一下
                return false;
            }
        }
    }

    return false;
}

bool FTPClient::list()
{
    if (m_isPassiveMode)
    {
        //TODO: 被动模式下的拉取文件列表信息稍后处理, by zhangxf 2025.05.19
        return false;
    }

    if (!m_bDataChannelConnected)
        return false;

    //发送MLSD命令
    std::string req("MLSD\r\n");
    if (!sendBuf(req))
    {
        close();
        return false;
    }

    if (!checkReadable())
    {
        close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!recvBuf(responseLines))
    {
        close();
        return false;
    }

    for (const auto& line : responseLines)
    {
        if (line.isEnd) {
            if (line.statusCode == FILE_STATUS_OKAY_ABOUT_TO_OPEN_DATA_CONNECTION)
                return true;
        }
    }

    return false;

}

bool FTPClient::sendBuf(std::string& buf)
{
    int n;

    while (true)
    {
        n = ::send(m_hControlSocket, buf.c_str(), buf.size(), 0);
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
            if (n == static_cast<int>(buf.size()))
            {
                LOGI("==>%s", buf.c_str());
                buf.clear();

                //数据都发完了
                return true;
            }
            else
            {
                //只发送了部分数据
                buf.erase(0, n);
            }
        }
    }
}

bool FTPClient::recvBuf(std::vector<ResponseLine>& responseLines)
{
    std::string buf;

    while (true)
    {
        char tmp[64] = { 0 };
        int bytesRecv = ::recv(m_hControlSocket, tmp, 64, 0);
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
        buf.append(tmp, bytesRecv);

        DecodePackageResult result = m_protocolParser.parseFTPResponse(buf, responseLines);
        if (result == DecodePackageResult::FAULT)
        {
            return false;
        }
        else if (result == DecodePackageResult::WANTMOREDATA)
        {
            continue;
        }
        else
        {
            //得到一个正确的响应
            return true;
        }
    }


    return true;
}

bool FTPClient::checkReadable(int timeoutSec/* = 3*/)
{
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(m_hControlSocket, &readset);
    struct timeval tv = { timeoutSec, 0 };
    int ret = ::select(m_hControlSocket + 1, &readset, NULL, NULL, &tv);

    return ret == 1;
}

bool FTPClient::parseDataIPAndPort(const std::string& responseLine)
{
    //227 Entering Passive Mode (127,0,0,1,249,38)

    size_t startBracketPos = responseLine.find("(");
    size_t endBracketPos = responseLine.find(")");

    if (startBracketPos == std::string::npos || endBracketPos == std::string::npos)
        return false;

    //TODO: 可以使用std::string_view去优化
    std::string ipAndPortStr = responseLine.substr(startBracketPos + 1, endBracketPos - startBracketPos - 1);

    std::vector<std::string> ipAndPort;
    StringUtil::split(ipAndPortStr, ipAndPort, ",");
    if (ipAndPort.size() != 6)
        return false;

    //127.0.0.1
    m_dataIP = ipAndPort[0] + "." + ipAndPort[1] + "." + ipAndPort[2] + "." + ipAndPort[3];

    int portV1 = atoi(ipAndPort[4].c_str());
    int portV2 = atoi(ipAndPort[5].c_str());
    if (portV1 < 0 || portV1 > 65535 || portV2 < 0 || portV2 > 65535)
        return false;

    m_dataPort = static_cast<uint16_t>(256 * portV1 + portV2);
    if (m_dataPort <= 0 || m_dataPort > 65535)
        return false;

    return true;

}

bool FTPClient::getDataServerAddr(std::string& dataIP, uint16_t& dataPort)
{
    if (!m_bControlChannelConnected || !m_bDataChannelConnected)
        return false;

    //通过控制连接的socket获取本机ip地址
    sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);
    int res = getsockname(m_hControlSocket, (sockaddr*)&addr, &addrLen);
    if (res == SOCKET_ERROR) {
        return false;
    }

    char hostbuf[NI_MAXHOST];
    char portbuf[NI_MAXSERV];

    res = getnameinfo((const SOCKADDR*)(&addr),
        addrLen, hostbuf, NI_MAXHOST,
        nullptr, 0,
        NI_NUMERICHOST | NI_NUMERICSERV);
    if (res != 0) {
        return false;
    }

    dataIP = hostbuf;

    //通过数据连接的socket获取监听的端口号
    sockaddr_storage dataServerAddr;
    addrLen = sizeof(dataServerAddr);
    res = getsockname(m_hDataSocket, (sockaddr*)&dataServerAddr, &addrLen);
    if (res == SOCKET_ERROR) {
        return false;
    }

    //getsockname这个函数获取的port是网络字节序，需要转成本机字节序
    dataPort = ntohs(((struct sockaddr_in*)&dataServerAddr)->sin_port);

    return true;
}

bool FTPClient::createDataServer()
{
    //1.创建一个侦听socket
    m_hDataSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_hDataSocket == SOCKET_ERROR)
    {
        LOGE("Failed to create data socket");
        return false;
    }

    //2.初始化服务器地址
    struct sockaddr_in bindaddr;
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //这里端口号设置为0，让操作系统自己分配一个可用的端口号
    bindaddr.sin_port = htons(0);
    if (bind(m_hDataSocket, (struct sockaddr*)&bindaddr, sizeof(bindaddr)) == -1)
    {
        LOGE("Failed to bind data socket");
        closesocket(m_hDataSocket);
        return false;
    }

    //3.启动侦听
    if (listen(m_hDataSocket, SOMAXCONN) == -1)
    {
        LOGE("Failed to listen on data socket");
        closesocket(m_hDataSocket);
        return false;
    }

    m_bDataChannelConnected = true;

    return true;
}

void FTPClient::setServerInfo(const std::string& ip, uint16_t port,
    const std::string& userName, const std::string& password,
    bool isPassiveMode)
{
    m_controlIP = ip;
    m_controlPort = port;

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

//void FTPClient::networkThreadFunc()
//{
//    while (m_running)
//    {
//        //建立socket连接
//        if (!m_bConnected)
//        {
//            if (connect())
//            {
//                LOGI("Connected to %s:%d", m_ip.c_str(), m_port);
//
//                m_clientState = FTPClientState::CONNECTED;
//            }
//            else
//            {
//                //TODO：反馈给UI层，网络连接不上，稍后重试
//
//                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
//
//                continue;
//            }
//        }
//
//
//        //判断是否有数据需要接收，有则收之，并解包，解包成功之后，反馈给UI层
//        fd_set readset;
//        FD_ZERO(&readset);
//        FD_SET(m_hSocket, &readset);
//        //struct timeval tv = { 0, 100000 };
//        struct timeval tv = { 1, 0 };
//        int ret = ::select(m_hSocket + 1, &readset, NULL, NULL, &tv);
//        if (ret == 0)
//        {
//            //select超时
//            //LOGI("No data...");
//        }
//        else if (ret == 1)
//        {
//            //有读事件
//            //收包
//            if (!recvBuf())
//            {
//                //关闭连接，等待重连
//                continue;
//            }
//
//            //解包
//            DecodePackageResult result = decodePackage();
//            if (result == DecodePackageResult::FAULT)
//            {
//                //TODO: 关闭连接，并重试
//                continue;
//            }
//            else if (result == DecodePackageResult::WANTMOREDATA)
//            {
//                continue;
//            }
//            else
//            {
//                //TODO: 成功拿到了包，交给界面展示即可
//                for (const auto& line : m_responseLines)
//                {
//                    LOGI("%d %s", line.statusCode, line.statusText.c_str());
//                }
//
//                if (!parseState())
//                {
//                    //TODO: 关闭连接，并重试
//                    continue;
//                }
//
//                m_responseLines.clear();
//            }
//
//        }
//        else
//        {
//            //TODO: select函数调用出错了，关闭socket进行重连
//            continue;
//        }
//
//
//        //查看是否有数据需要发送，有则发送
//        switch (m_clientState)
//        {
//        case FTPClientState::WELCOMEMSGRECEIVED:
//            //走登录逻辑，发用户名
//            m_sendBuf.append("USER " + m_userName + "\r\n");
//            break;
//
//        case FTPClientState::USERNAMEOKAYNEEDPASSWORD:
//            //走登录逻辑，发密码
//            m_sendBuf.append("PASS " + m_password + "\r\n");
//            break;
//
//        case FTPClientState::LOGON:
//            //登录成功
//            if (m_isPassiveMode)
//            {
//                //修改成被动模式
//                m_sendBuf.append("PASV\r\n");
//            }
//            break;
//
//        default:
//            //
//            continue;
//        }
//
//        if (!sendBuf())
//        {
//            //关闭连接重试
//        }
//    }
//}

void FTPClient::close()
{
    ::closesocket(m_hControlSocket);

    m_bControlChannelConnected = false;
}