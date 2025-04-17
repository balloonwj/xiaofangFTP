/**
 * @desc:   字符检测类，CharChecker.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#pragma once

class CharChecker final
{
public:
    static bool isDigit(char c);


private:
    CharChecker() = delete;
    ~CharChecker() = delete;
    CharChecker(const CharChecker& rhs) = delete;
    CharChecker& operator=(const CharChecker& rhs) = delete;
};

