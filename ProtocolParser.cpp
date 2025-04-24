/**
 * @desc:   FTP协议解析类，ProtocolParser.cpp
 * @author: zhangxf
 * @date:   2025.04.20
 */

#include "ProtocolParser.h"

#define STATUS_CODE_LENGTH                    3
#define SPACE_OR_MINUS_LENGTH                 1
#define FTP_RESPONSE_LINE_END_FLAG_LENGTH     2

 //允许的最大的Response响应行的文本长度
#define MAX_RESPONSE_LINE_TEXT_LENGTH         256


#define MAKE_STATUA_CODE(ch1, ch2, ch3)             \
        (static_cast<int16_t>(ch1 - '0') * 100 +    \
        static_cast<int16_t>(ch2 - '0') * 10 +      \
        static_cast<int16_t>(ch3 - '0'))

 /**
     220-Unknown\r\n
     220-written by Tim Kosse (tim.kosse@filezilla-project.org)\r\n
     220 Please visit https://filezilla-project.org/\r\n

     502 Explicit TLS authentication not allowed\r\n
 */
DecodePackageResult ProtocolParser::parseFTPResponse(std::string& buf, std::vector<ResponseLine>& responseLines)
{
    char p;
    size_t bufLen = buf.length();
    size_t currentTextLength = 0;
    for (; m_parsePos < bufLen; ++m_parsePos)
    {
        p = buf[m_parsePos];

        switch (m_parseState)
        {
        case FTP_PARSE_STATE::FTP_PARSE_START:
            if (p >= '1' && p <= '9')
            {
                m_statusFirstLetter = p;
                m_parseState = FTP_PARSE_STATE::FTP_PARSE_STATUS_CODE_FISRT_LETTER;
            }
            else
            {
                return DecodePackageResult::FAULT;
            }
            break;

        case FTP_PARSE_STATE::FTP_PARSE_STATUS_CODE_FISRT_LETTER:
            if (p >= '0' && p <= '9')
            {
                m_statusSecondLetter = p;
                m_parseState = FTP_PARSE_STATE::FTP_PARSE_STATUS_CODE_SECOND_LETTER;
            }
            else
            {
                return DecodePackageResult::FAULT;
            }
            break;

        case FTP_PARSE_STATE::FTP_PARSE_STATUS_CODE_SECOND_LETTER:
            if (p >= '0' && p <= '9')
            {
                m_statusThirdLetter = p;
                m_parseState = FTP_PARSE_STATE::FTP_PARSE_STATUS_CODE_THIRD_LETTER;
            }
            else
            {
                return DecodePackageResult::FAULT;
            }
            break;

        case FTP_PARSE_STATE::FTP_PARSE_STATUS_CODE_THIRD_LETTER:
            if (p == ' ')
            {
                if (m_isParsingMultilineResponse)
                {
                    //多行响应的结束行
                    m_isMultilineEnd = true;
                    m_parseState = FTP_PARSE_STATE::FTP_PARSE_STATUS_SPACE_BEFORE_TEXT;
                }
                else
                {
                    //单行响应
                    m_isSingleResponse = true;
                    m_parseState = FTP_PARSE_STATE::FTP_PARSE_STATUS_SPACE_BEFORE_TEXT;
                }
            }
            else if (p == '-')
            {
                //多行响应
                m_isSingleResponse = false;
                m_isMultilineEnd = false;
                m_parseState = FTP_PARSE_STATE::FTP_PARSE_STATUS_MINUS_BEFORE_TEXT;

                m_isParsingMultilineResponse = true;
            }
            else
            {
                return DecodePackageResult::FAULT;
            }
            break;

        case FTP_PARSE_STATE::FTP_PARSE_STATUS_SPACE_BEFORE_TEXT:
        case FTP_PARSE_STATE::FTP_PARSE_STATUS_MINUS_BEFORE_TEXT:
            if (p != '\r' && p != '\n')
            {
                currentTextLength++;
                m_responseTextStart = m_parsePos;
                m_parseState = FTP_PARSE_STATE::FTP_PARSE_STATUS_TEXT;
            }
            else
            {
                return DecodePackageResult::FAULT;
            }
            break;

        case FTP_PARSE_STATE::FTP_PARSE_STATUS_TEXT:
            if (p == '\r')
            {
                m_responseTextEnd = m_parsePos - 1;
                m_parseState = FTP_PARSE_STATE::FTP_PARSE_STATUS_ALMOST_END;
            }
            else
            {
                currentTextLength++;
                if (currentTextLength > MAX_RESPONSE_LINE_TEXT_LENGTH)
                {
                    //单行报文文本过长，视为不合法的包
                    return DecodePackageResult::FAULT;
                }
            }

            break;

        case FTP_PARSE_STATE::FTP_PARSE_STATUS_ALMOST_END:
            if (p == '\n')
            {
                m_parseState = FTP_PARSE_STATE::FTP_PARSE_STATUS_END;

                if (m_isSingleResponse)
                {
                    ResponseLine responseLine;
                    responseLine.statusCode =
                        MAKE_STATUA_CODE(m_statusFirstLetter, m_statusSecondLetter, m_statusThirdLetter);

                    responseLine.statusText = buf.substr(m_responseTextStart, m_responseTextEnd - m_responseTextStart + 1);
                    responseLine.isEnd = true;
                    responseLines.push_back(responseLine);

                    resetState();
                    //继续解析下一个包
                }
                else
                {
                    ResponseLine responseLine;
                    responseLine.statusCode =
                        MAKE_STATUA_CODE(m_statusFirstLetter, m_statusSecondLetter, m_statusThirdLetter);

                    responseLine.statusText = buf.substr(m_responseTextStart, m_responseTextEnd - m_responseTextStart + 1);
                    if (m_isMultilineEnd)
                        responseLine.isEnd = true;
                    else
                        responseLine.isEnd = false;

                    responseLines.push_back(responseLine);

                    resetState();
                }
            }
            else
            {
                return DecodePackageResult::FAULT;
            }

            break;

        default:
            return DecodePackageResult::FAULT;

        }//end switch
    }//end for-loop


    size_t charCount = 0;
    for (const auto& line : responseLines)
    {
        charCount += line.statusText.length() + STATUS_CODE_LENGTH +
            SPACE_OR_MINUS_LENGTH + FTP_RESPONSE_LINE_END_FLAG_LENGTH;

        if (line.isEnd)
        {
            buf.erase(0, charCount);

            m_parsePos = 0;

            return DecodePackageResult::SUCCESS;
        }

    }

    return DecodePackageResult::WANTMOREDATA;

}//end func ProtocolParser::parseFTPResponse2


void ProtocolParser::resetState()
{
    m_parseState = FTP_PARSE_STATE::FTP_PARSE_START;
}