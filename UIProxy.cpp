/**
 * @desc:   UI层所有任务集中通过这个类处理，UIProxy.h
 * @author: zhangxf
 * @date:   2025.04.10
 */

#include "UIProxy.h"

#include "ConnectTask.h"
#include "CwdTask.h"
#include "EnterPassiveModeTask.h"
#include "ListTask.h"
#include "LogonTask.h"
#include "PortTask.h"
#include "Processor.h"
#include "PwdTask.h"

UIProxy& UIProxy::getInstance()
{
    static UIProxy proxy;
    return proxy;
}

void UIProxy::connect(const std::wstring& ip, uint16_t port,
    const std::wstring& userName, std::wstring& password,
    bool isPassiveMode)
{
    ConnectTask* pConnectTask = new ConnectTask(ip, port, userName, password, isPassiveMode);
    Processor::getInstance().addSendTask(pConnectTask);

    LogonTask* pLogonTask = new LogonTask();
    Processor::getInstance().addSendTask(pLogonTask);

    PwdTask* pPwdTask = new PwdTask();
    Processor::getInstance().addSendTask(pPwdTask);

    if (isPassiveMode)
    {
        EnterPassiveModeTask* pEnterPassiveModeTask = new EnterPassiveModeTask();
        Processor::getInstance().addSendTask(pEnterPassiveModeTask);
    }

    ////TODO: 仅用于测试，后续移除
    //CwdTask* pCwdTask = new CwdTask("xiaofangFTP");
    //Processor::getInstance().addSendTask(pCwdTask);

    //TODO: 仅用于测试，后续移除
    PortTask* pPortTask = new PortTask();
    Processor::getInstance().addSendTask(pPortTask);

    ListTask* pListTask = new ListTask();
    Processor::getInstance().addSendTask(pListTask);
}