// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_ADDRESS_H
#define MINEMON_ADDRESS_H

#include "destination.h"

namespace minemon
{

class CAddress : public CDestination
{
public:
    enum
    {
        ADDRESS_LEN = 57
    };

    CAddress(const CDestination& destIn = CDestination());
    CAddress(const std::string& strAddress);
    virtual ~CAddress() = default;

    virtual bool ParseString(const std::string& strAddress);
    virtual std::string ToString() const;
};

} // namespace minemon

#endif //CMINEMON_ADDRESS_H
