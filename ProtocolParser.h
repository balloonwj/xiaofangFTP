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

enum class FTP_PARSE_STATE
{
    FTP_PARSE_START,
    FTP_PARSE_STATUS_CODE_FISRT_LETTER,
    FTP_PARSE_STATUS_CODE_SECOND_LETTER,
    FTP_PARSE_STATUS_CODE_THIRD_LETTER,
    FTP_PARSE_STATUS_SPACE_BEFORE_TEXT,
    FTP_PARSE_STATUS_MINUS_BEFORE_TEXT,
    FTP_PARSE_STATUS_TEXT,
    FTP_PARSE_STATUS_ALMOST_END, //\r
    FTP_PARSE_STATUS_END         //\n
};

struct ResponseLine
{
    int16_t     statusCode;
    std::string statusText;
    bool        isEnd;
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
     //DecodePackageResult parseFTPResponse(std::string& buf, std::vector<std::string>& resultLines);

     /**原位解析FTP报文
      */
    DecodePackageResult parseFTPResponse(std::string& buf, std::vector<ResponseLine>& responseLines);

private:
    void resetState();

private:
    //上一次解析的进度状态
    FTP_PARSE_STATE m_parseState{ FTP_PARSE_STATE::FTP_PARSE_START };
    //上一次解析的buf位置
    size_t          m_parsePos{ 0 };

    char            m_statusFirstLetter;
    char            m_statusSecondLetter;
    char            m_statusThirdLetter;
    bool            m_isSingleResponse;
    //正文在buf中的起始和结束位置
    size_t          m_responseTextStart{ 0 };
    size_t          m_responseTextEnd{ 0 };
    //是否正在解析一个多行包
    bool            m_isParsingMultilineResponse;
    //是否是多行响应的最后一行
    bool            m_isMultilineEnd;
};
