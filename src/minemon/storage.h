// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_STORAGE_H
#define MINEMON_STORAGE_H

#include <boost/filesystem.hpp>
#include <xengine.h>

#include "config.h"

namespace minemon
{

class CEntry : public xengine::CEntry
{
public:
    CEntry();
    ~CEntry();
    bool Initialize(int argc, char* argv[]);
    bool Run();
    void Exit();

protected:
    bool InitializeService();
    bool InitializeClient();
    //    xengine::CHttpHostConfig GetRPCHostConfig();
    //    xengine::CHttpHostConfig GetWebUIHostConfig();

    boost::filesystem::path GetDefaultDataDir();

    bool SetupEnvironment();
    bool RunInBackground(const boost::filesystem::path& pathData);
    void ExitBackground(const boost::filesystem::path& pathData);

protected:
    CConfig config;
    xengine::CLog log;
    xengine::CDocker docker;
};

} // namespace minemon

#endif //MINEMON_STORAGE_H
