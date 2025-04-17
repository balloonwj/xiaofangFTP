/**
 * @desc:   FTP协议解析类，ProtocolParser.h
 * @author: zhangxf
 * @date:   2025.04.17
 */

#pragma once

#include <string>
#include <vector>

enum class DecodePackageResult
{
    SUCCESS,
    FAULT,
    WANTMOREDATA
};

class ProtocolParser
{
public:
    ProtocolParser() = default;
    ~ProtocolParser() = default;

    /** 解析协议
     * @param buf，待解析的协议数据buf，解析成功后，该buf变成剩余的数据，如果没有剩余则为空
     * @pram resultLines, 解析成功后，存放每一行解析好的FTP响应
     * @return 返回解析成功、出错和数据不够三种情况
     */
    DecodePackageResult parseFTPResponse(std::string& buf, std::vector<std::string>& resultLines);
};
