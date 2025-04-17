/**
 * @desc:   UI层所有任务集中通过这个类处理，UIProxy.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#include "UIProxy.h"

#include "ConnectTask.h"
#include "Processor.h"

UIProxy& UIProxy::getInstance()
{
    static UIProxy proxy;
    return proxy;
}

void UIProxy::connect(const std::wstring& ip, uint16_t port, const std::wstring& userName, std::wstring& password)
{
    ConnectTask* pTask = new ConnectTask(ip, port, userName, password);

    Processor::getInstance().addSendTask(pTask);
}