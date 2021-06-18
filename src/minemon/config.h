// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_CONFIG_H
#define MINEMON_CONFIG_H

#include <string>
#include <type_traits>
#include <vector>

#include "mode/mode.h"
#include "xengine.h"

namespace minemon
{
class CConfig
{
public:
    CConfig();
    ~CConfig();
    bool Load(int argc, char* argv[], const boost::filesystem::path& pathDefault,
              const std::string& strConfile);
    bool PostLoad();
    void ListConfig() const;
    std::string Help() const;

    inline CBasicConfig* GetConfig()
    {
        return pImpl;
    }
    inline EModeType GetModeType()
    {
        return emMode;
    }

protected:
    EModeType emMode;
    std::string subCmd;
    CBasicConfig* pImpl;
};

} // namespace minemon

#endif // MINEMON_CONFIG_H
