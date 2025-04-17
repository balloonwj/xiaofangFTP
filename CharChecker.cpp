/**
 * @desc:   字符检测类，CharChecker.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#include "CharChecker.h"

bool CharChecker::isDigit(char c)
{
    switch (c)
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return true;

    default:
        return false;
    }
}