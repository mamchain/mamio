// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_RPCCLIENT_H
#define MINEMON_RPCCLIENT_H

#include "json/json_spirit_value.h"
#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <thread>

#include "base.h"
#include "rpc/rpc.h"
#include "xengine.h"

namespace minemon
{

class CRPCClient : public xengine::IIOModule, virtual public xengine::CHttpEventListener
{
public:
    CRPCClient(bool fConsole = true);
    ~CRPCClient();
    void ConsoleHandleLine(const std::string& strLine);
    ;

protected:
    bool HandleInitialize() override;
    void HandleDeinitialize() override;
    bool HandleInvoke() override;
    void HandleHalt() override;
    const CRPCClientConfig* Config();

    bool HandleEvent(xengine::CEventHttpGetRsp& event) override;
    bool GetResponse(uint64 nNonce, const std::string& content);
    bool CallRPC(rpc::CRPCParamPtr spParam, int nReqId);
    bool CallConsoleCommand(const std::vector<std::string>& vCommand);
    void LaunchConsole();
    void LaunchCommand();
    void CancelCommand();

    void EnterLoop();
    void LeaveLoop();

protected:
    xengine::IIOProc* pHttpGet;
    xengine::CThread thrDispatch;
    std::vector<std::string> vArgs;
    uint64 nLastNonce;
    xengine::CIOCompletion ioComplt;
    std::atomic<bool> isConsoleRunning;

};

} // namespace minemon
#endif //MINEMON_RPCCLIENT_H
