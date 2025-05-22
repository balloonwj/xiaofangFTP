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
#include "UploadTask.h"

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

    //ListTask* pListTask = new ListTask();
    //Processor::getInstance().addSendTask(pListTask);

    UploadTask* pUploadTask = new UploadTask("E:\\CppPractice\\36-37-38-39-40-41-42-43-44-45-46-47-48-49-50-51-52-53-54-55-56-57-58-59th\\pdf\\cpp17indetail.pdf", "cpp17indetail.pdf");
    Processor::getInstance().addSendTask(pUploadTask);
}