// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_MODE_MODE_TYPE_H
#define MINEMON_MODE_MODE_TYPE_H

namespace minemon
{
// mode type
enum class EModeType
{
    ERROR = 0, // ERROR type
    SERVER,    // server
    CONSOLE,   // console
    MINER,     // miner
};

} // namespace minemon

#endif // MINEMON_MODE_MODE_TYPE_H
