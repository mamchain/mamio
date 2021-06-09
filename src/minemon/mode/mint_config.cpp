// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mode/mint_config.h"

#include "address.h"
#include "mode/config_macro.h"

namespace minemon
{
namespace po = boost::program_options;

CMintConfig::CMintConfig()
{
    po::options_description desc("MinemonMint");

    CMintConfigOption::AddOptionsImpl(desc);

    AddOptions(desc);
}

CMintConfig::~CMintConfig() {}

bool CMintConfig::PostLoad()
{
    if (fHelp)
    {
        return true;
    }
    CAddress address;
    if (address.ParseString(strSpentAddress) && !address.IsNull())
    {
        destSpent = address;
    }
    return true;
}

std::string CMintConfig::ListConfig() const
{
    std::ostringstream oss;
    oss << CMintConfigOption::ListConfigImpl();
    oss << "destSpent: " << CAddress(destSpent).ToString() << "\n";
    oss << "nPledgeFee: " << nPledgeFee << "\n";
    return oss.str();
}

std::string CMintConfig::Help() const
{
    return CMintConfigOption::HelpImpl();
}

} // namespace minemon
