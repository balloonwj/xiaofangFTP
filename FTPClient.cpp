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

FTPClient& FTPClient::getInstance()
{
    static FTPClient ftpServer;
    return ftpServer;
}

auto FTPClient::connect(const std::string& ip, uint16_t port, int timeoutSec /*= 3*/)
{
    m_hControlSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_hControlSocket == INVALID_SOCKET)
        return false;

    long tmSend = 3 * 1000L;
    long tmRecv = 3 * 1000L;
    long noDelay = 1;
    ::setsockopt(m_hControlSocket, IPPROTO_TCP, TCP_NODELAY, (LPSTR)&noDelay, sizeof(long));
    ::setsockopt(m_hControlSocket, SOL_SOCKET, SO_SNDTIMEO, (LPSTR)&tmSend, sizeof(long));
    ::setsockopt(m_hControlSocket, SOL_SOCKET, SO_RCVTIMEO, (LPSTR)&tmRecv, sizeof(long));

    //将socket设置成非阻塞的
    unsigned long on = 1;
    if (::ioctlsocket(m_hControlSocket, FIONBIO, &on) == SOCKET_ERROR)
    {
        return false;
    }

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
    struct timeval tv = { timeoutSec, 0 };
    if (::select(m_hControlSocket + 1, NULL, &writeset, NULL, &tv) != 1)
    {
        //LOG_ERROR("Could not connect to server:%s, port:%d.", m_strServer.c_str(), m_nPort);
        return false;
    }

    m_bControlChannelConnected = true;

    return true;
}

bool FTPClient::connectWithResponse(const std::string& ip, uint16_t port)
{
    ConnectInfo connInfo;
    connInfo.controlIP = ip;
    connInfo.controlPort = port;

    auto [connInfo.hControlSock, connInfo.controlChannelConnected] = connect(ip, port);

    if (!connInfo.controlChannelConnected)
    {
        connInfo.close();
        return false;
    }

    if (!connInfo.checkReadable(connInfo.hControlSock))
    {
        connInfo.close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!connInfo.recvAndDecodeBufOnControlSocket(responseLines))
    {
        connInfo.close();
        return false;
    }

    for (const auto& line : responseLines)
    {
        if (line.isEnd && line.statusCode == SERVICE_READY_FOR_NEW_USER)
        {
            m_connectInfo.emplace(connInfo.hControlSock, connInfo);
            return true;
        }

    }

    connInfo.close();

    return false;
}

bool FTPClient::logon(int id, const std::string& userName, const std::string& password)
{
    auto iter = m_connectInfo.find(id);
    if (iter == m_connectInfo.end())
        return false;

    //发送用户名
    std::string req("USER ");
    req.append(userName);
    req.append("\r\n");

    //发送密码
    req = "PASS ";
    req.append(password);
    req.append("\r\n");

    std::vector<ResponseLine> responseLines;
    return iter->second.request(req, USER_NAME_OKAY_NEED_PASSWORD, nullptr) &&
        iter->second.request(req, USER_LOGGED_IN, nullptr);
}

bool FTPClient::pwd(int id, std::string& currPath)
{
    auto iter = m_connectInfo.find(id);
    if (iter == m_connectInfo.end())
        return false;

    //发送PWD命令
    std::string req("PWD\r\n");
    return iter->second.request(req, PATHNAME_CREATED, &currPath);
}

bool FTPClient::pasv(int id, std::string& dataIPAndPort)
{
    auto iter = m_connectInfo.find(id);
    if (iter == m_connectInfo.end())
        return false;

    //发送PASV命令
    std::string req("PASV\r\n");
    return iter->second.request(req, ENTERING_PASSIVE_MODE, &dataIPAndPort);
}

bool FTPClient::port(int id)
{
    auto iter = m_connectInfo.find(id);
    if (iter == m_connectInfo.end())
        return false;

    if (!iter->second.createDataServer())
        return false;

    //将ip格式从127.0.0.1替换成127,0,0,1
    std::string tmpIPStr(iter->second.dataIP);
    StringUtil::replace(tmpIPStr, ".", ",");

    char reqStr[32] = { 0 };
    sprintf_s(reqStr, "PORT %s,%d,%d\r\n", tmpIPStr.c_str(), iter->second.dataPort / 256, iter->second.dataPort % 256);

    //发送数据格式：
    //PORT 127,0,0,1,211,219
    std::string req(reqStr);
    return iter->second.request(req, COMMAND_OKAY, nullptr);
}

bool FTPClient::cwd(int id, const std::string& targetDir)
{
    auto iter = m_connectInfo.find(id);
    if (iter == m_connectInfo.end())
        return false;

    //发送CWD命令
    std::string req("CWD ");
    req.append(targetDir);
    req.append("\r\n");

    return iter->second.request(req, REQUESTED_FILE_ACTION_OKAY_COMPLETED, nullptr);
}

bool FTPClient::mkdir(int id, const std::string& serverDirName)
{
    auto iter = m_connectInfo.find(id);
    if (iter == m_connectInfo.end())
        return false;

    //发送DELE命令
    std::string req("MKD ");
    req.append(serverDirName);
    req.append("\r\n");

    return iter->second.request(req, PATHNAME_CREATED, nullptr);
}

bool FTPClient::rename(int id, const std::string& serverOldFileName, const std::string& serverNewFileName)
{
    auto iter = m_connectInfo.find(id);
    if (iter == m_connectInfo.end())
        return false;

    //发送RNFR命令
    std::string req("RNFR ");
    req.append(serverOldFileName);
    req.append("\r\n");

    //发送RNTO命令
    std::string req2("RNTO ");
    req2.append(serverNewFileName);
    req2.append("\r\n");

    return iter->second.request(req, REQUESTED_FILE_ACTION_PENDING_FURTHER_INFORMATION, nullptr) &&
        iter->second.request(req, REQUESTED_FILE_ACTION_OKAY_COMPLETED, nullptr);
}

bool FTPClient::del(int id, const std::string& targetFileOrDir)
{
    auto iter = m_connectInfo.find(id);
    if (iter == m_connectInfo.end())
        return false;

    //发送DELE命令
    std::string req("DELE ");
    req.append(targetFileOrDir);
    req.append("\r\n");
    return iter->second.request(req, REQUESTED_FILE_ACTION_OKAY_COMPLETED, nullptr);
}

bool FTPClient::type(int id, bool ascii)
{
    auto iter = m_connectInfo.find(id);
    if (iter == m_connectInfo.end())
        return false;

    //发送TYPE命令
    std::string req("TYPE ");
    if (ascii)
        req.append("A");
    else
        req.append("I");
    req.append("\r\n");

    return iter->second.request(req, COMMAND_OKAY, nullptr);
}

bool FTPClient::list(int id)
{
    if (m_isPassiveMode)
        return listInPassiveMode();

    return listInActiveMode();
}

bool FTPClient::upload(const std::string& localFilePath, const std::string& serverFileName)
{
    if (m_isPassiveMode)
        return uploadInPassiveMode(localFilePath, serverFileName);

    return uploadInActiveMode(localFilePath, serverFileName);
}

bool FTPClient::download(const std::string& localFilePath, const std::string& serverFileName)
{
    if (m_isPassiveMode)
        return downloadInPassiveMode(localFilePath, serverFileName);

    return downloadInActiveMode(localFilePath, serverFileName);
}





bool FTPClient::parseDataIPAndPort(const std::string& responseLine, std::string& dataIP, uint16_t dataPort)
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
    dataIP = ipAndPort[0] + "." + ipAndPort[1] + "." + ipAndPort[2] + "." + ipAndPort[3];

    int portV1 = atoi(ipAndPort[4].c_str());
    int portV2 = atoi(ipAndPort[5].c_str());
    if (portV1 < 0 || portV1 > 65535 || portV2 < 0 || portV2 > 65535)
        return false;

    dataPort = static_cast<uint16_t>(256 * portV1 + portV2);
    if (m_dataPort <= 0 || m_dataPort > 65535)
        return false;

    return true;

}





bool FTPClient::parseDirEntries(const std::string& dirInfo, std::vector<DirEntry>& entries)
{
    //解析如下格式：
    //type=file;modify=20250520130731;size=0; 1.txt\r\ntype=dir;modify=20250520130820; 2\r\ntype=file;modify=20250520130831;size=0; 3.png\r\n

    std::vector<std::string> v;
    StringUtil::split(dirInfo, v, "\r\n");
    if (v.empty())
        return false;

    size_t modifyPos;
    const int MODIFY_PREFIX_LENGTH = strlen("modify=");
    const int SIZE_PREFIX_LENGTH = strlen("size=");
    const size_t DIR_INFO_COUNT = 3;
    const size_t FILE_INFO_COUNT = 4;
    for (const auto& iter : v)
    {
        //iter格式：
        //type=file;modify=20250520130731;size=0; 1.txt
        std::vector<std::string> v2;
        StringUtil::split(iter, v2, ";");
        //目录类型为3个元素，文件类型为4个元素
        if (v2.size() != DIR_INFO_COUNT && v2.size() != FILE_INFO_COUNT)
            continue;

        DirEntry entry;
        if (v2[0] == "type=file")
        {
            //文件串格式：type=file;modify=20250520130731;size=0; 1.txt
            //拆分之后有4个元素
            entry.fileType = FileType::File;
            entry.modify = v2[1].substr(MODIFY_PREFIX_LENGTH);

            std::string sizeStr = v2[2].substr(SIZE_PREFIX_LENGTH);
            entry.size = atoll(sizeStr.c_str());

            entry.name = v2[3].substr(1);
        }
        else if (v2[0] == "type=dir")
        {
            //目录串格式：type=dir;modify=20250520130820; 2
            //拆分之后有3个元素
            entry.fileType = FileType::Dir;
            entry.modify = v2[1].substr(MODIFY_PREFIX_LENGTH);;
            entry.name = v2[2].substr(1);
        }
        else
            continue;

        entries.push_back(entry);
    }

    return true;
}

bool FTPClient::sendBytes(SOCKET s, char* buf, int bufLen)
{
    int pos = 0;
    while (true)
    {
        int n = send(s, buf + pos, bufLen - pos, 0);
        if (n == 0)
        {
            if (pos == bufLen)
                return true;
            else
                return false;
        }
        else if (n > 0)
        {
            pos += n;
            if (pos == bufLen)
                return true;

            continue;
        }
        else {
            // n<0
            if (WSAGetLastError() == WSAEWOULDBLOCK)
                continue;
            else
                return false;
        }
    }
}

bool FTPClient::listInActiveMode()
{
    if (!m_bDataChannelConnected)
        return false;

    //发送MLSD命令
    std::string req("MLSD\r\n");
    if (!sendBuf(req))
    {
        close();
        return false;
    }

    if (!checkReadable(m_hControlSocket))
    {
        close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!recvAndDecodeBuf(responseLines))
    {
        close();
        return false;
    }

    bool success = false;
    for (const auto& line : responseLines)
    {
        if (line.isEnd) {
            if (line.statusCode == FILE_STATUS_OKAY_ABOUT_TO_OPEN_DATA_CONNECTION)
            {
                success = true;
                break;
            }
        }
    }

    if (!success)
        return true;

    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);
    //4. 接受客户端连接
    m_hDataSocket = accept(m_hDataListenSocket, (struct sockaddr*)&clientaddr, &clientaddrlen);
    if (m_hDataSocket < 0)
        return false;

    u_long argp = 1;
    ioctlsocket(m_hDataSocket, FIONBIO, &argp);

    int n;
    std::string dataRecvBuf;
    while (true)
    {
        char buf[4096] = { 0 };
        n = recv(m_hDataSocket, buf, sizeof(buf), 0);
        if (n > 0)
        {
            dataRecvBuf.append(buf, n);
        }
        else if (n < 0)
        {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
            {
                continue;
            }
            else
            {
                closesocket(m_hDataListenSocket);
                closesocket(m_hDataSocket);

                m_bDataChannelConnected = false;

                return false;
            }
        }
        else
        {
            //recv函数返回0时表明对端已经发完数据了
            closesocket(m_hDataListenSocket);
            closesocket(m_hDataSocket);

            m_bDataChannelConnected = false;

            break;
        }
    }

    //解析目录数据
    std::vector<DirEntry> entries;
    parseDirEntries(dataRecvBuf, entries);

    LOGI("received dir info:");
    for (const auto& entry : entries)
    {
        LOGI("name: %s, type: %s, size: %lld, modify: %s",
            entry.name.c_str(),
            entry.fileType == FileType::File ? "file" : "dir",
            entry.size,
            entry.modify);
    }

    return true;
}

bool FTPClient::listInPassiveMode()
{
    if (!m_bDataChannelConnected)
        return false;

    //发送MLSD命令
    std::string req("MLSD\r\n");
    if (!sendBuf(req))
    {
        close();
        return false;
    }

    if (!checkReadable(m_hControlSocket))
    {
        close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!recvAndDecodeBuf(responseLines))
    {
        close();
        return false;
    }

    bool success = false;
    for (const auto& line : responseLines)
    {
        if (line.isEnd) {
            if (line.statusCode == FILE_STATUS_OKAY_ABOUT_TO_OPEN_DATA_CONNECTION)
            {
                success = true;
                break;
            }
        }
    }

    if (!success)
        return true;

    m_hDataSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_hControlSocket == INVALID_SOCKET)
        return false;

    long tmSend = 3 * 1000L;
    long tmRecv = 3 * 1000L;
    long noDelay = 1;
    setsockopt(m_hDataSocket, IPPROTO_TCP, TCP_NODELAY, (LPSTR)&noDelay, sizeof(long));
    setsockopt(m_hDataSocket, SOL_SOCKET, SO_SNDTIMEO, (LPSTR)&tmSend, sizeof(long));
    setsockopt(m_hDataSocket, SOL_SOCKET, SO_RCVTIMEO, (LPSTR)&tmRecv, sizeof(long));

    //将socket设置成非阻塞的
    unsigned long on = 1;
    if (::ioctlsocket(m_hDataSocket, FIONBIO, &on) == SOCKET_ERROR)
    {
        closesocket(m_hDataSocket);
        return false;
    }

    struct sockaddr_in addrSrv = { 0 };
    struct hostent* pHostent = NULL;
    unsigned int addr = 0;

    if ((addrSrv.sin_addr.s_addr = inet_addr(m_dataIP.c_str())) == INADDR_NONE)
    {
        pHostent = ::gethostbyname(m_dataIP.c_str());
        if (!pHostent)
        {
            closesocket(m_hDataSocket);
            //LOG_ERROR("Could not connect server:%s, port:%d.", m_strServer.c_str(), port);
            return false;
        }
        else
            addrSrv.sin_addr.s_addr = *((unsigned long*)pHostent->h_addr);
    }

    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons((u_short)m_dataPort);
    int ret = ::connect(m_hDataSocket, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
    if (ret == 0)
    {
        //LOG_INFO("Connect to server:%s, port:%d successfully.", m_strServer.c_str(), m_nPort);
        m_bDataChannelConnected = true;
    }
    else if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
    {
        closesocket(m_hDataSocket);
        //LOG_ERROR("Could not connect to server:%s, port:%d.", m_strServer.c_str(), m_nPort);
        return false;
    }

    fd_set writeset;
    FD_ZERO(&writeset);
    FD_SET(m_hDataSocket, &writeset);
    struct timeval tv = { 3, 0 };
    if (::select(m_hDataSocket + 1, NULL, &writeset, NULL, &tv) != 1)
    {
        closesocket(m_hDataSocket);
        //LOG_ERROR("Could not connect to server:%s, port:%d.", m_strServer.c_str(), m_nPort);
        return false;
    }

    m_bControlChannelConnected = true;

    int n;
    std::string dataRecvBuf;
    while (true)
    {
        char buf[4096] = { 0 };
        n = recv(m_hDataSocket, buf, sizeof(buf), 0);
        if (n > 0)
        {
            dataRecvBuf.append(buf, n);
        }
        else if (n < 0)
        {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
            {
                continue;
            }
            else
            {
                closesocket(m_hDataSocket);

                m_bDataChannelConnected = false;

                return false;
            }
        }
        else //n=0
        {
            //recv函数返回0时表明对端已经发完数据了
            closesocket(m_hDataSocket);

            m_bDataChannelConnected = false;

            break;
        }
    }

    //解析目录数据
    std::vector<DirEntry> entries;
    parseDirEntries(dataRecvBuf, entries);

    LOGI("received dir info:");
    for (const auto& entry : entries)
    {
        LOGI("name: %s, type: %s, size: %lld, modify: %s",
            entry.name.c_str(),
            entry.fileType == FileType::File ? "file" : "dir",
            entry.size,
            entry.modify);
    }

    return true;
}

bool FTPClient::uploadInActiveMode(const std::string& localFilePath, const std::string& serverFileName)
{
    if (!m_bDataChannelConnected)
        return false;

    //发送STOR命令
    std::string req("STOR ");
    req += serverFileName;
    req += "\r\n";
    if (!sendBuf(req))
    {
        close();
        return false;
    }

    if (!checkReadable(m_hControlSocket))
    {
        close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!recvAndDecodeBuf(responseLines))
    {
        close();
        return false;
    }

    bool success = false;
    for (const auto& line : responseLines)
    {
        if (line.isEnd) {
            if (line.statusCode == FILE_STATUS_OKAY_ABOUT_TO_OPEN_DATA_CONNECTION)
            {
                success = true;
                break;
            }
        }
    }

    if (!success)
        return true;

    m_hDataSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_hControlSocket == INVALID_SOCKET)
        return false;

    long tmSend = 3 * 1000L;
    long tmRecv = 3 * 1000L;
    long noDelay = 1;
    setsockopt(m_hDataSocket, IPPROTO_TCP, TCP_NODELAY, (LPSTR)&noDelay, sizeof(long));
    setsockopt(m_hDataSocket, SOL_SOCKET, SO_SNDTIMEO, (LPSTR)&tmSend, sizeof(long));
    setsockopt(m_hDataSocket, SOL_SOCKET, SO_RCVTIMEO, (LPSTR)&tmRecv, sizeof(long));

    //将socket设置成非阻塞的
    unsigned long on = 1;
    if (::ioctlsocket(m_hDataSocket, FIONBIO, &on) == SOCKET_ERROR)
    {
        closesocket(m_hDataSocket);
        return false;
    }

    struct sockaddr_in addrSrv = { 0 };
    struct hostent* pHostent = NULL;
    unsigned int addr = 0;

    if ((addrSrv.sin_addr.s_addr = inet_addr(m_dataIP.c_str())) == INADDR_NONE)
    {
        pHostent = ::gethostbyname(m_dataIP.c_str());
        if (!pHostent)
        {
            closesocket(m_hDataSocket);
            //LOG_ERROR("Could not connect server:%s, port:%d.", m_strServer.c_str(), port);
            return false;
        }
        else
            addrSrv.sin_addr.s_addr = *((unsigned long*)pHostent->h_addr);
    }

    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons((u_short)m_dataPort);
    int retx = ::connect(m_hDataSocket, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
    if (retx == 0)
    {
        //LOG_INFO("Connect to server:%s, port:%d successfully.", m_strServer.c_str(), m_nPort);
        m_bDataChannelConnected = true;
    }
    else if (retx == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
    {
        closesocket(m_hDataSocket);
        //LOG_ERROR("Could not connect to server:%s, port:%d.", m_strServer.c_str(), m_nPort);
        return false;
    }

    fd_set writeset;
    FD_ZERO(&writeset);
    FD_SET(m_hDataSocket, &writeset);
    struct timeval tv = { 3, 0 };
    if (::select(m_hDataSocket + 1, NULL, &writeset, NULL, &tv) != 1)
    {
        closesocket(m_hDataSocket);
        //LOG_ERROR("Could not connect to server:%s, port:%d.", m_strServer.c_str(), m_nPort);
        return false;
    }

    m_bControlChannelConnected = true;
    //打开文件，读一段发一段，发完之后关闭数据连接的发通道

    HANDLE hFile = CreateFileA(localFilePath.c_str(), GENERIC_READ,
        FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        return false;
    }

    DWORD fileSizeHigh;
    DWORD fileSizeLow = GetFileSize(hFile, &fileSizeHigh);
    if (fileSizeLow == INVALID_FILE_SIZE)
    {
        CloseHandle(hFile);

        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        return false;
    }

    int64_t fileSize = ((static_cast<int64_t>(fileSizeHigh)) << 32) | fileSizeLow;

    int64_t eachBytesToRead = 2048;
    char fileBuf[2048];
    DWORD bytesRead;
    bool error = false;
    bool ret;
    int64_t remainingBytes = fileSize;
    while (true)
    {
        if (remainingBytes <= eachBytesToRead)
            eachBytesToRead = remainingBytes;

        if (!ReadFile(hFile, fileBuf,
            eachBytesToRead,
            &bytesRead, NULL) || eachBytesToRead != bytesRead)
        {
            error = true;
            break;
        }

        ret = sendBytes(m_hDataSocket, fileBuf, eachBytesToRead);
        if (!ret)
        {
            error = true;
            break;
        }

        remainingBytes = remainingBytes - eachBytesToRead;

        LOGI("serverFileName: %s, remaining bytes: %lld", serverFileName.c_str(), remainingBytes);

        //数据已经发完
        if (remainingBytes == 0)
            break;
    }

    CloseHandle(hFile);

    if (error)
    {
        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        LOGE("serverFileName: %s upload failed.", serverFileName.c_str());
        return false;
    }


    //注意: 这里不能直接关闭，如果直接关闭可能导致m_hDataSocket上的数据在操作系统内核还来不及发出去
    // 导致filezilla server收到的数据不完整
    //closesocket(m_hDataListenSocket);
    //closesocket(m_hDataSocket);

    shutdown(m_hDataSocket, SD_SEND);

    if (checkReadable(m_hDataSocket))
    {
        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        LOGE("select error, errno: %u", WSAGetLastError());

        return false;
    }

    char buf[32];
    int n = recv(m_hDataSocket, buf, 32, 0);
    if (n != 0)
    {
        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        LOGE("upload failed");

        return false;
    }

    closesocket(m_hDataListenSocket);
    closesocket(m_hDataSocket);

    m_bDataChannelConnected = false;

    if (!checkReadable(m_hControlSocket))
    {
        close();
        return false;
    }

    responseLines.clear();
    if (!recvAndDecodeBuf(responseLines))
    {
        close();
        return false;
    }

    for (const auto& line : responseLines)
    {
        if (line.isEnd) {
            if (line.statusCode == CLOSING_DATA_CONNECTION_AND_REQUESTED_FILE_ACTION_SUCCESSFUL)
            {
                LOGI("serverFileName: %s upload successfully.", serverFileName.c_str());
                return true;
            }
        }
    }

    LOGE("serverFileName: %s upload failed, response is not expected.", serverFileName.c_str());

    return false;
}

bool FTPClient::uploadInPassiveMode(const std::string& localFilePath, const std::string& serverFileName)
{
    if (!m_bDataChannelConnected)
        return false;

    //发送STOR命令
    std::string req("STOR ");
    req += serverFileName;
    req += "\r\n";
    if (!sendBuf(req))
    {
        close();
        return false;
    }

    if (!checkReadable(m_hControlSocket))
    {
        close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!recvAndDecodeBuf(responseLines))
    {
        close();
        return false;
    }

    bool success = false;
    for (const auto& line : responseLines)
    {
        if (line.isEnd) {
            if (line.statusCode == FILE_STATUS_OKAY_ABOUT_TO_OPEN_DATA_CONNECTION)
            {
                success = true;
                break;
            }
        }
    }

    if (!success)
        return true;

    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);
    //4. 接受客户端连接
    m_hDataSocket = accept(m_hDataListenSocket, (struct sockaddr*)&clientaddr, &clientaddrlen);
    if (m_hDataSocket < 0)
        return false;

    u_long argp = 1;
    ioctlsocket(m_hDataSocket, FIONBIO, &argp);

    //打开文件，读一段发一段，发完之后关闭数据连接的发通道

    HANDLE hFile = CreateFileA(localFilePath.c_str(), GENERIC_READ,
        FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        return false;
    }

    DWORD fileSizeHigh;
    DWORD fileSizeLow = GetFileSize(hFile, &fileSizeHigh);
    if (fileSizeLow == INVALID_FILE_SIZE)
    {
        CloseHandle(hFile);

        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        return false;
    }

    int64_t fileSize = ((static_cast<int64_t>(fileSizeHigh)) << 32) | fileSizeLow;

    int64_t eachBytesToRead = 2048;
    char fileBuf[2048];
    DWORD bytesRead;
    bool error = false;
    bool ret;
    int64_t remainingBytes = fileSize;
    while (true)
    {
        if (remainingBytes <= eachBytesToRead)
            eachBytesToRead = remainingBytes;

        if (!ReadFile(hFile, fileBuf,
            eachBytesToRead,
            &bytesRead, NULL) || eachBytesToRead != bytesRead)
        {
            error = true;
            break;
        }

        ret = sendBytes(m_hDataSocket, fileBuf, eachBytesToRead);
        if (!ret)
        {
            error = true;
            break;
        }

        remainingBytes = remainingBytes - eachBytesToRead;

        LOGI("serverFileName: %s, remaining bytes: %lld", serverFileName.c_str(), remainingBytes);

        //数据已经发完
        if (remainingBytes == 0)
            break;
    }

    CloseHandle(hFile);

    if (error)
    {
        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        LOGE("serverFileName: %s upload failed.", serverFileName.c_str());
        return false;
    }


    //注意: 这里不能直接关闭，如果直接关闭可能导致m_hDataSocket上的数据在操作系统内核还来不及发出去
    // 导致filezilla server收到的数据不完整
    //closesocket(m_hDataListenSocket);
    //closesocket(m_hDataSocket);

    shutdown(m_hDataSocket, SD_SEND);

    if (checkReadable(m_hDataSocket))
    {
        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        LOGE("select error, errno: %u", WSAGetLastError());

        return false;
    }

    char buf[32];
    int n = recv(m_hDataSocket, buf, 32, 0);
    if (n != 0)
    {
        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        LOGE("upload failed");

        return false;
    }

    closesocket(m_hDataListenSocket);
    closesocket(m_hDataSocket);

    m_bDataChannelConnected = false;

    if (!checkReadable(m_hControlSocket))
    {
        close();
        return false;
    }

    responseLines.clear();
    if (!recvAndDecodeBuf(responseLines))
    {
        close();
        return false;
    }

    for (const auto& line : responseLines)
    {
        if (line.isEnd) {
            if (line.statusCode == CLOSING_DATA_CONNECTION_AND_REQUESTED_FILE_ACTION_SUCCESSFUL)
            {
                LOGI("serverFileName: %s upload successfully.", serverFileName.c_str());
                return true;
            }
        }
    }

    LOGE("serverFileName: %s upload failed, response is not expected.", serverFileName.c_str());

    return false;
}

bool FTPClient::downloadInActiveMode(const std::string& localFilePath, const std::string& serverFileName)
{
    if (!m_bDataChannelConnected)
        return false;

    //发送RETR命令
    std::string req("RETR ");
    req += serverFileName;
    req += "\r\n";
    if (!sendBuf(req))
    {
        close();
        return false;
    }

    if (!checkReadable(m_hControlSocket))
    {
        close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!recvAndDecodeBuf(responseLines))
    {
        close();
        return false;
    }

    bool success = false;
    for (const auto& line : responseLines)
    {
        if (line.isEnd) {
            if (line.statusCode == FILE_STATUS_OKAY_ABOUT_TO_OPEN_DATA_CONNECTION)
            {
                success = true;
                break;
            }
        }
    }

    if (!success)
        return true;

    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);
    //4. 接受客户端连接
    m_hDataSocket = accept(m_hDataListenSocket, (struct sockaddr*)&clientaddr, &clientaddrlen);
    if (m_hDataSocket < 0)
        return false;

    u_long argp = 1;
    ioctlsocket(m_hDataSocket, FIONBIO, &argp);

    //打开文件，读一段发一段，发完之后关闭数据连接的发通道

    HANDLE hFile = CreateFileA(localFilePath.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        return false;
    }

    bool error = false;
    while (true)
    {
        char buf[2048];
        int n = recv(m_hDataSocket, buf, sizeof(buf), 0);
        if (n == SOCKET_ERROR)
        {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
            {
                error = true;
                break;
            }
            else
            {
                continue;
            }
        }
        else if (n > 0)
        {
            DWORD dwBytesWritten;
            if (!WriteFile(hFile, buf, n, &dwBytesWritten, NULL) ||
                dwBytesWritten != static_cast<DWORD>(n))
            {
                error = true;
                break;
            }
        }
        else //n=0
        {
            break;
        }
    }


    if (error)
    {
        CloseHandle(hFile);

        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        LOGE("serverFileName: %s download failed.", serverFileName.c_str());
        return false;
    }

    FlushFileBuffers(hFile);
    CloseHandle(hFile);

    closesocket(m_hDataListenSocket);
    closesocket(m_hDataSocket);

    m_bDataChannelConnected = false;

    if (!checkReadable(m_hControlSocket))
    {
        close();
        return false;
    }

    responseLines.clear();
    if (!recvAndDecodeBuf(responseLines))
    {
        close();
        return false;
    }

    for (const auto& line : responseLines)
    {
        if (line.isEnd) {
            if (line.statusCode == CLOSING_DATA_CONNECTION_AND_REQUESTED_FILE_ACTION_SUCCESSFUL)
            {
                LOGI("serverFileName: %s download successfully.", serverFileName.c_str());
                return true;
            }
        }
    }

    LOGE("serverFileName: %s download failed, response is not expected.", serverFileName.c_str());

    return false;
}

bool FTPClient::downloadInPassiveMode(const std::string& localFilePath, const std::string& serverFileName)
{
    if (!m_bDataChannelConnected)
        return false;

    //发送RETR命令
    std::string req("RETR ");
    req += serverFileName;
    req += "\r\n";
    if (!sendBuf(req))
    {
        close();
        return false;
    }

    if (!checkReadable(m_hControlSocket))
    {
        close();
        return false;
    }

    std::vector<ResponseLine> responseLines;
    if (!recvAndDecodeBuf(responseLines))
    {
        close();
        return false;
    }

    bool success = false;
    for (const auto& line : responseLines)
    {
        if (line.isEnd) {
            if (line.statusCode == FILE_STATUS_OKAY_ABOUT_TO_OPEN_DATA_CONNECTION)
            {
                success = true;
                break;
            }
        }
    }

    if (!success)
        return true;

    m_hDataSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_hControlSocket == INVALID_SOCKET)
        return false;

    long tmSend = 3 * 1000L;
    long tmRecv = 3 * 1000L;
    long noDelay = 1;
    setsockopt(m_hDataSocket, IPPROTO_TCP, TCP_NODELAY, (LPSTR)&noDelay, sizeof(long));
    setsockopt(m_hDataSocket, SOL_SOCKET, SO_SNDTIMEO, (LPSTR)&tmSend, sizeof(long));
    setsockopt(m_hDataSocket, SOL_SOCKET, SO_RCVTIMEO, (LPSTR)&tmRecv, sizeof(long));

    //将socket设置成非阻塞的
    unsigned long on = 1;
    if (::ioctlsocket(m_hDataSocket, FIONBIO, &on) == SOCKET_ERROR)
    {
        closesocket(m_hDataSocket);
        return false;
    }

    struct sockaddr_in addrSrv = { 0 };
    struct hostent* pHostent = NULL;
    unsigned int addr = 0;

    if ((addrSrv.sin_addr.s_addr = inet_addr(m_dataIP.c_str())) == INADDR_NONE)
    {
        pHostent = ::gethostbyname(m_dataIP.c_str());
        if (!pHostent)
        {
            closesocket(m_hDataSocket);
            //LOG_ERROR("Could not connect server:%s, port:%d.", m_strServer.c_str(), port);
            return false;
        }
        else
            addrSrv.sin_addr.s_addr = *((unsigned long*)pHostent->h_addr);
    }

    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons((u_short)m_dataPort);
    int ret = ::connect(m_hDataSocket, (struct sockaddr*)&addrSrv, sizeof(addrSrv));
    if (ret == 0)
    {
        //LOG_INFO("Connect to server:%s, port:%d successfully.", m_strServer.c_str(), m_nPort);
        m_bDataChannelConnected = true;
    }
    else if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
    {
        closesocket(m_hDataSocket);
        //LOG_ERROR("Could not connect to server:%s, port:%d.", m_strServer.c_str(), m_nPort);
        return false;
    }

    fd_set writeset;
    FD_ZERO(&writeset);
    FD_SET(m_hDataSocket, &writeset);
    struct timeval tv = { 3, 0 };
    if (::select(m_hDataSocket + 1, NULL, &writeset, NULL, &tv) != 1)
    {
        closesocket(m_hDataSocket);
        //LOG_ERROR("Could not connect to server:%s, port:%d.", m_strServer.c_str(), m_nPort);
        return false;
    }

    m_bControlChannelConnected = true;

    //打开文件，读一段发一段，发完之后关闭数据连接的发通道

    HANDLE hFile = CreateFileA(localFilePath.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        return false;
    }

    bool error = false;
    while (true)
    {
        char buf[2048];
        int n = recv(m_hDataSocket, buf, sizeof(buf), 0);
        if (n == SOCKET_ERROR)
        {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
            {
                error = true;
                break;
            }
            else
            {
                continue;
            }
        }
        else if (n > 0)
        {
            DWORD dwBytesWritten;
            if (!WriteFile(hFile, buf, n, &dwBytesWritten, NULL) ||
                dwBytesWritten != static_cast<DWORD>(n))
            {
                error = true;
                break;
            }
        }
        else //n=0
        {
            break;
        }
    }


    if (error)
    {
        CloseHandle(hFile);

        closesocket(m_hDataListenSocket);
        closesocket(m_hDataSocket);

        m_bDataChannelConnected = false;

        LOGE("serverFileName: %s download failed.", serverFileName.c_str());
        return false;
    }

    FlushFileBuffers(hFile);
    CloseHandle(hFile);

    closesocket(m_hDataListenSocket);
    closesocket(m_hDataSocket);

    m_bDataChannelConnected = false;

    if (!checkReadable(m_hControlSocket))
    {
        close();
        return false;
    }

    responseLines.clear();
    if (!recvAndDecodeBuf(responseLines))
    {
        close();
        return false;
    }

    for (const auto& line : responseLines)
    {
        if (line.isEnd) {
            if (line.statusCode == CLOSING_DATA_CONNECTION_AND_REQUESTED_FILE_ACTION_SUCCESSFUL)
            {
                LOGI("serverFileName: %s download successfully.", serverFileName.c_str());
                return true;
            }
        }
    }

    LOGE("serverFileName: %s download failed, response is not expected.", serverFileName.c_str());

    return false;
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

void FTPClient::close()
{

}