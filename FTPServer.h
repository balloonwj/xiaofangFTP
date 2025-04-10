/**
 * @desc:   网络接口API，FTPServer.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#ifndef FTP_SERVER_H_
#define FTP_SERVER_H_

#include <cstdint>
#include <string>

class FTPServer final
{
public:
    FTPServer();
    ~FTPServer();

    FTPServer(const FTPServer& rhs) = delete;
    FTPServer(FTPServer&& rhs) = delete;

    FTPServer& operator=(const FTPServer& rhs) = delete;
    FTPServer& operator=(FTPServer&& rhs) = delete;

public:
    bool logon(const char* ip, uint16_t port, const char* username, const char* password);
    std::string list();

    bool cwd();

    bool upload();
    bool download();

    bool setMode(bool passiveMode);

};



#endif //!FTP_SERVER_H_
