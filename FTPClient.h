/**
 * @desc:   网络接口API，FTPClient.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#ifndef FTP_CLIENT_H_
#define FTP_CLIENT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include <WinSock2.h>

#include "ProtocolParser.h"

enum class FTPClientState
{
    DISCONNECTED,
    CONNECTED,
    WELCOMEMSGRECEIVED,
    USERNAMEOKAYNEEDPASSWORD,
    LOGON
};

struct DirEntry
{
    std::string name;
};

class FTPClient final
{
public:
    static FTPClient& getInstance();

public:
    void startNetworkThread();
    void stopNetworkThread();

    bool connectWithResponse();

    bool logon(/*const char* username, const char* password*/);

    std::string pwd();

    bool pasv();

    bool port();

    bool cwd(const std::string& targetDir);

    bool del(const std::string& targetFileOrDir);

    bool list();



    bool upload();
    bool download();

    bool setMode(bool passiveMode);



    void setServerInfo(const std::string& ip, uint16_t port,
        const std::string& userName, const std::string& password,
        bool isPassiveMode);

private:
    FTPClient();
    ~FTPClient();

    FTPClient(const FTPClient& rhs) = delete;
    FTPClient(FTPClient&& rhs) = delete;

    FTPClient& operator=(const FTPClient& rhs) = delete;
    FTPClient& operator=(FTPClient&& rhs) = delete;

private:
    bool connect(int timeout = 3);

    void close();

    bool sendBuf(std::string& buf);
    bool recvBuf(std::vector<ResponseLine>& responseLines);

    //判断是否有数据需要接收
    bool checkReadable(int timeoutSec = 3);

    bool parseDataIPAndPort(const std::string& responseLine);

    bool getDataServerAddr(std::string& dataIP, uint16_t& dataPort);

    //创建数据通道监听server
    bool createDataServer();

    DecodePackageResult decodePackage();

    bool parseState();


private:
    std::unique_ptr<std::thread>        m_spNetworkThread;

    std::string                         m_controlIP;
    uint16_t                            m_controlPort;
    std::string                         m_userName;
    std::string                         m_password;
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

    ProtocolParser                      m_protocolParser;

    std::vector<ResponseLine>           m_responseLines;

    //数据通道socket
    SOCKET                              m_hDataSocket{ INVALID_SOCKET };

    bool                                m_bDataChannelConnected{ false };
};



#endif //!FTP_CLIENT_H_
