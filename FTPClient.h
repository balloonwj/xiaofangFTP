/**
 * @desc:   网络接口API，FTPClient.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#ifndef FTP_CLIENT_H_
#define FTP_CLIENT_H_

#include <cstdint>
#include <unordered_map>
#include <memory>
#include <string>
#include <thread>

#include <WinSock2.h>

#include "ProtocolParser.h"

enum FTP_STATUS_CODE
{
    FILE_STATUS_OKAY_ABOUT_TO_OPEN_DATA_CONNECTION = 150,   //数据传输端口已经ready，准备开始传输数据   
    COMMAND_OKAY = 200,
    SERVICE_READY_FOR_NEW_USER = 220,
    CLOSING_DATA_CONNECTION_AND_REQUESTED_FILE_ACTION_SUCCESSFUL = 226,
    USER_NAME_OKAY_NEED_PASSWORD = 331,
    USER_LOGGED_IN = 230,
    PATHNAME_CREATED = 257, //显示当前路径成功
    ENTERING_PASSIVE_MODE = 227, //进入被动模式
    REQUESTED_FILE_ACTION_OKAY_COMPLETED = 250, //进入目录成功
    REQUESTED_FILE_ACTION_PENDING_FURTHER_INFORMATION = 350,
    REQUESTED_ACTION_NOT_TAKEN = 550, //权限不足或者文件不存在
};

enum class FTPClientState
{
    DISCONNECTED,
    CONNECTED,
    WELCOMEMSGRECEIVED,
    USERNAMEOKAYNEEDPASSWORD,
    LOGON
};

enum class FileType
{
    File,
    Dir
};

struct DirEntry
{
    std::string name;
    int64_t     size;
    std::string modify;
    FileType    fileType;
};

struct ConnectInfo
{
    std::string     userName;
    std::string     password;
    SOCKET          hControlSock{ INVALID_SOCKET };
    std::string     controlIP;
    uint16_t        controlPort;
    bool            controlChannelConnected{ false };

    SOCKET          hDataSock{ INVALID_SOCKET };
    SOCKET          hDataListenSock;
    std::string     dataIP;
    uint16_t        dataPort;
    bool            dataChannelConnected{ false };

    bool            passiveMode;

    ProtocolParser  m_protocolParser;

    bool checkReadable(SOCKET s, int timeoutSec = 3)
    {
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(s, &readset);
        struct timeval tv = { timeoutSec, 0 };
        int ret = ::select(s + 1, &readset, NULL, NULL, &tv);

        return ret == 1;
    }

    bool request(std::string& reqBuf, FTP_STATUS_CODE expectedStatusCode, std::string* returnedData = nullptr)
    {
        if (!controlChannelConnected)
            return false;

        if (!_sendBufOnControlSocket(reqBuf))
        {
            return false;
        }

        if (!checkReadable(hControlSock))
        {
            return false;
        }

        std::vector<ResponseLine> responseLines;
        if (!_recvAndDecodeBufOnControlSocket(responseLines))
        {
            return false;
        }

        for (const auto& line : responseLines)
        {
            if (line.isEnd && line.statusCode == expectedStatusCode)
                return true;
        }

        return false;
    }

    void close()
    {
        ::closesocket(hControlSock);

        hControlSock = INVALID_SOCKET;

        controlChannelConnected = false;
    }

    bool createDataServer()
    {
        //1.创建一个侦听socket
        hDataSock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (hDataSock == SOCKET_ERROR)
        {
            //LOGE("Failed to create data socket");
            return false;
        }

        //2.初始化服务器地址
        struct sockaddr_in bindaddr;
        bindaddr.sin_family = AF_INET;
        bindaddr.sin_addr.s_addr = ::htonl(INADDR_ANY);
        //这里端口号设置为0，让操作系统自己分配一个可用的端口号
        bindaddr.sin_port = ::htons(0);
        if (::bind(hDataSock, (struct sockaddr*)&bindaddr, sizeof(bindaddr)) == -1)
        {
            //LOGE("Failed to bind data socket");
            closesocket(hDataSock);
            return false;
        }

        //3.启动侦听
        if (::listen(hDataSock, SOMAXCONN) == -1)
        {
            //LOGE("Failed to listen on data socket");
            ::closesocket(hDataSock);
            return false;
        }

        dataChannelConnected = true;

        if (!getDataServerAddr(dataIP, dataPort))
        {
            ::closesocket(hDataSock);
            return false;
        }

        return true;
    }

    bool getDataServerAddr(std::string& dataIP, uint16_t& dataPort)
    {
        if (!controlChannelConnected || !dataChannelConnected)
            return false;

        //通过控制连接的socket获取本机ip地址
        sockaddr_storage addr;
        socklen_t addrLen = sizeof(addr);
        int res = getsockname(hControlSock, (sockaddr*)&addr, &addrLen);
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
        res = ::getsockname(hDataListenSock, (sockaddr*)&dataServerAddr, &addrLen);
        if (res == SOCKET_ERROR) {
            return false;
        }

        //getsockname这个函数获取的port是网络字节序，需要转成本机字节序
        dataPort = ::ntohs(((struct sockaddr_in*)&dataServerAddr)->sin_port);

        return true;
    }

private:
    bool _sendBufOnControlSocket(std::string& buf)
    {
        int n;

        while (true)
        {
            n = ::send(hControlSock, buf.c_str(), buf.size(), 0);
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
                    //LOGI("==>%s", buf.c_str());
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

    bool _recvAndDecodeBufOnControlSocket(std::vector<ResponseLine>& responseLines)
    {
        std::string buf;

        while (true)
        {
            char tmp[64] = { 0 };
            int bytesRecv = ::recv(hControlSock, tmp, 64, 0);
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
};

class FTPClient final
{
public:
    static FTPClient& getInstance();

public:
    bool connectWithResponse(const std::string& ip, uint16_t port);

    bool logon(int id, const std::string& userName, const std::string& password);

    bool pwd(int id, std::string& currPath);

    bool pasv(int id, std::string& dataIPAndPort);

    bool port(int id);

    bool cwd(int id, const std::string& targetDir);

    bool mkdir(int id, const std::string& serverDirName);

    bool rename(int id, const std::string& serverOldFileName, const std::string& serverNewFileName);

    bool del(int id, const std::string& targetFileOrDir);

    bool type(int id, bool ascii);

    bool list(int id);

    bool upload(int id, const std::string& localFilePath, const std::string& serverFileName);
    bool download(int id, const std::string& localFilePath, const std::string& serverFileName);

private:
    FTPClient();
    ~FTPClient();

    FTPClient(const FTPClient& rhs) = delete;
    FTPClient& operator=(const FTPClient& rhs) = delete;

private:
    auto connect(const std::string& ip, uint16_t port, int timeoutSec = 3);

    void close();

    bool sendBuf(std::string& buf);
    bool recvAndDecodeBuf(std::vector<ResponseLine>& responseLines);

    //判断是否有数据需要接收
    bool checkReadable(SOCKET s, int timeoutSec = 3);

    bool parseDataIPAndPort(const std::string& responseLine, std::string& dataIP, uint16_t dataPort);

    bool getDataServerAddr(std::string& dataIP, uint16_t& dataPort);

    //创建数据通道监听server
    bool createDataServer();

    bool parseDirEntries(const std::string& dirInfo, std::vector<DirEntry>& entries);

    //用于非阻塞socket把数据发完
    bool sendBytes(SOCKET s, char* buf, int bufLen);

    bool listInActiveMode();
    bool listInPassiveMode();

    bool uploadInActiveMode(const std::string& localFilePath, const std::string& serverFileName);
    bool uploadInPassiveMode(const std::string& localFilePath, const std::string& serverFileName);

    bool downloadInActiveMode(const std::string& localFilePath, const std::string& serverFileName);
    bool downloadInPassiveMode(const std::string& localFilePath, const std::string& serverFileName);


private:
    std::unordered_map<SOCKET, ConnectInfo> m_connectInfo;

    std::string                         m_controlIP;
    uint16_t                            m_controlPort;
    std::string                         m_dataIP;
    uint16_t                            m_dataPort;
    bool                                m_isPassiveMode;

    bool                                m_running{ false };

    FTPClientState                      m_clientState{ FTPClientState::DISCONNECTED };

    //控制通道socket
    SOCKET                              m_hControlSocket;

    bool                                m_bControlChannelConnected{ false };

    //收发缓冲区
    std::string                         m_sendBuf;
    std::string                         m_recvBuf;



    std::vector<ResponseLine>           m_responseLines;

    //数据通道监听socket
    SOCKET                              m_hDataListenSocket{ INVALID_SOCKET };
    SOCKET                              m_hDataSocket{ INVALID_SOCKET };

    bool                                m_bDataChannelConnected{ false };
};



#endif //!FTP_CLIENT_H_
