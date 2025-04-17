/**
 * @desc:   UI层所有任务集中通过这个类处理，UIProxy.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#pragma once

#include <string>

class UIProxy {
public:
    static UIProxy& getInstance();

    void connect(const std::wstring& ip, uint16_t port, const std::wstring& userName, std::wstring& password);

private:
    UIProxy() = default;
    ~UIProxy() = default;

};
