/**
 * @desc:   网络接口API，FTPServer.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#ifndef FTP_SERVER_H_
#define FTP_SERVER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include <WinSock2.h>

#include "ProtocolParser.h"

class FTPServer final
{
public:
    static FTPServer& getInstance();

public:
    void startNetworkThread();
    void stopNetworkThread();


    bool logon(const char* ip, uint16_t port, const char* username, const char* password);
    std::string list();

    bool cwd();

    bool upload();
    bool download();

    bool setMode(bool passiveMode);

    bool connect(const std::string& ip, uint16_t port, int timeout = 3);

    bool recvBuf();

private:
    FTPServer();
    ~FTPServer();

    FTPServer(const FTPServer& rhs) = delete;
    FTPServer(FTPServer&& rhs) = delete;

    FTPServer& operator=(const FTPServer& rhs) = delete;
    FTPServer& operator=(FTPServer&& rhs) = delete;

private:
    void networkThreadFunc();


private:
    std::unique_ptr<std::thread>        m_spNetworkThread;

    bool                                m_running{ false };

    SOCKET                              m_hSocket;

    bool                                m_bConnected{ false };

    std::string                         m_recvBuf;

    ProtocolParser                      m_protocolParser;
};



#endif //!FTP_SERVER_H_
