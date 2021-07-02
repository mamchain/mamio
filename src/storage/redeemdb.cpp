// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "redeemdb.h"

#include <boost/range/adaptor/reversed.hpp>

#include "leveldbeng.h"

#include "block.h"
#include "param.h"
#include "template/template.h"

using namespace std;
using namespace xengine;

namespace minemon
{
namespace storage
{

//////////////////////////////
// CRedeemDB

bool CRedeemDB::Initialize(const boost::filesystem::path& pathData)
{
    CLevelDBArguments args;
    args.path = (pathData / "redeem").string();
    args.syncwrite = false;
    CLevelDBEngine* engine = new CLevelDBEngine(args);

    if (!Open(engine))
    {
        delete engine;
        return false;
    }
    return true;
}

void CRedeemDB::Deinitialize()
{
    mapCacheFullRedeem.clear();
    mapCacheIncRedeem.clear();
    Close();
}

bool CRedeemDB::AddNew(const uint256& hashBlock, const CRedeemContext& ctxtRedeem)
{
    xengine::CWriteLock wlock(rwData);
    return AddRedeem(hashBlock, ctxtRedeem);
}

bool CRedeemDB::Remove(const uint256& hashBlock)
{
    xengine::CWriteLock wlock(rwData);
    return RemoveRedeem(hashBlock);
}

bool CRedeemDB::Retrieve(const uint256& hashBlock, CRedeemContext& ctxtRedeem)
{
    xengine::CReadLock rlock(rwData);
    if (!GetRedeem(hashBlock, ctxtRedeem))
    {
        StdError("CRedeemDB", "Retrieve: Get redeem fail, block: %s", hashBlock.GetHex().c_str());
        return false;
    }
    if (!ctxtRedeem.IsFull())
    {
        std::map<CDestination, CDestRedeem> mapIncRedeem;
        mapIncRedeem = ctxtRedeem.mapRedeem;
        ctxtRedeem.mapRedeem.clear();
        if (!GetRedeem(ctxtRedeem.hashRef, ctxtRedeem.mapRedeem))
        {
            StdError("CRedeemDB", "Retrieve: Get redeem fail, ref: %s", ctxtRedeem.hashRef.GetHex().c_str());
            return false;
        }
        for (const auto& kv : mapIncRedeem)
        {
            ctxtRedeem.mapRedeem[kv.first] = kv.second;
        }
        ctxtRedeem.hashRef = 0;
    }
    return true;
}

bool CRedeemDB::AddBlockRedeem(const uint256& hashBlock, const uint256& hashPrev, const std::vector<std::pair<CDestination, int64>>& vTxRedeemIn)
{
    xengine::CWriteLock wlock(rwData);

    CRedeemContext ctxtRedeem;
    if (IsFullRedeem(hashBlock))
    {
        if (!GetFullBlockRedeem(hashBlock, hashPrev, vTxRedeemIn, ctxtRedeem))
        {
            StdError("CRedeemDB", "Add block redeem: Add full redeem fail");
            return false;
        }
    }
    else
    {
        if (!GetIncrementBlockRedeem(hashBlock, hashPrev, vTxRedeemIn, ctxtRedeem))
        {
            StdError("CRedeemDB", "Add block redeem: Add increment redeem fail");
            return false;
        }
    }

    if (!AddRedeem(hashBlock, ctxtRedeem))
    {
        StdError("CRedeemDB", "Add block redeem: Add redeem fail");
        return false;
    }
    return true;
}

bool CRedeemDB::GetBlockRedeem(const uint256& hashBlock, const uint256& hashPrev, const std::vector<std::pair<CDestination, int64>>& vTxRedeemIn, CRedeemContext& ctxtRedeem)
{
    xengine::CReadLock rlock(rwData);
    if (IsFullRedeem(hashBlock))
    {
        if (!GetFullBlockRedeem(hashBlock, hashPrev, vTxRedeemIn, ctxtRedeem))
        {
            return false;
        }
    }
    else
    {
        if (!GetIncrementBlockRedeem(hashBlock, hashPrev, vTxRedeemIn, ctxtRedeem))
        {
            return false;
        }
    }
    return true;
}

bool CRedeemDB::RetrieveBlockRedeem(const uint256& hashBlock, CRedeemContext& ctxtRedeem)
{
    xengine::CReadLock rlock(rwData);
    return GetRedeem(hashBlock, ctxtRedeem);
}

bool CRedeemDB::RetrieveAddressRedeem(const uint256& hashBlock, const CDestination& dest, CDestRedeem& destRedeem)
{
    xengine::CReadLock rlock(rwData);
    return GetAddressRedeem(hashBlock, dest, destRedeem);
}

void CRedeemDB::Clear()
{
    mapCacheFullRedeem.clear();
    mapCacheIncRedeem.clear();
    RemoveAll();
}

///////////////////////////////////////////////
bool CRedeemDB::IsFullRedeem(const uint256& hashBlock)
{
    if (CBlock::GetBlockHeightByHash(hashBlock) == 0)
    {
        return true;
    }
    return ((CBlock::GetBlockHeightByHash(hashBlock) % BPX_PLEDGE_REWARD_DISTRIBUTE_HEIGHT) == 1);
}

bool CRedeemDB::GetFullBlockRedeem(const uint256& hashBlock, const uint256& hashPrev, const std::vector<std::pair<CDestination, int64>>& vTxRedeemIn, CRedeemContext& ctxtRedeem)
{
    if (!GetRedeem(hashPrev, ctxtRedeem))
    {
        StdError("CRedeemDB", "Get full: Get redeem fail");
        return false;
    }

    if (!ctxtRedeem.IsFull())
    {
        std::map<CDestination, CDestRedeem> mapIncRedeem;
        mapIncRedeem = ctxtRedeem.mapRedeem;
        ctxtRedeem.mapRedeem.clear();
        if (!GetRedeem(ctxtRedeem.hashRef, ctxtRedeem.mapRedeem))
        {
            StdError("CRedeemDB", "Get full: Get redeem fail, ref: %s", ctxtRedeem.hashRef.GetHex().c_str());
            return false;
        }
        for (const auto& kv : mapIncRedeem)
        {
            ctxtRedeem.mapRedeem[kv.first] = kv.second;
        }
    }
    ctxtRedeem.hashRef = 0;

    for (const auto& kv : vTxRedeemIn)
    {
        if (kv.second > 0)
        {
            auto& redeem = ctxtRedeem.mapRedeem[kv.first];
            redeem.nBalance += kv.second;
            redeem.nLockAmount = redeem.nBalance;
            redeem.nLockBeginHeight = CBlock::GetBlockHeightByHash(hashBlock);
        }
        else if (kv.second < 0)
        {
            ctxtRedeem.mapRedeem[kv.first].nBalance += kv.second;
        }
    }
    for (auto it = ctxtRedeem.mapRedeem.begin(); it != ctxtRedeem.mapRedeem.end();)
    {
        if (it->second.nBalance <= 0)
        {
            ctxtRedeem.mapRedeem.erase(it++);
        }
        else
        {
            ++it;
        }
    }
    return true;
}

bool CRedeemDB::GetIncrementBlockRedeem(const uint256& hashBlock, const uint256& hashPrev, const std::vector<std::pair<CDestination, int64>>& vTxRedeemIn, CRedeemContext& ctxtRedeem)
{
    if (!GetRedeem(hashPrev, ctxtRedeem))
    {
        StdError("CRedeemDB", "Get increment: Get redeem fail");
        return false;
    }

    std::map<CDestination, CDestRedeem> mapRefFullRedeem;
    if (ctxtRedeem.IsFull())
    {
        mapRefFullRedeem = ctxtRedeem.mapRedeem;
        ctxtRedeem.hashRef = hashPrev;
        ctxtRedeem.mapRedeem.clear();
    }
    else
    {
        if (!GetRedeem(ctxtRedeem.hashRef, mapRefFullRedeem))
        {
            StdError("CRedeemDB", "Get increment: Get redeem fail");
            return false;
        }
    }

    for (const auto& kv : vTxRedeemIn)
    {
        if (kv.second != 0)
        {
            auto it = ctxtRedeem.mapRedeem.find(kv.first);
            if (it == ctxtRedeem.mapRedeem.end())
            {
                it = ctxtRedeem.mapRedeem.insert(make_pair(kv.first, CDestRedeem())).first;
                auto mt = mapRefFullRedeem.find(kv.first);
                if (mt != mapRefFullRedeem.end())
                {
                    it->second = mt->second;
                }
            }
            auto& redeem = it->second;
            if (kv.second > 0)
            {
                redeem.nBalance += kv.second;
                redeem.nLockAmount = redeem.nBalance;
                redeem.nLockBeginHeight = CBlock::GetBlockHeightByHash(hashBlock);
            }
            else
            {
                redeem.nBalance += kv.second;
            }
        }
    }
    return true;
}

bool CRedeemDB::AddRedeem(const uint256& hashBlock, const CRedeemContext& ctxtRedeem)
{
    if (!Write(hashBlock, ctxtRedeem))
    {
        StdError("CRedeemDB", "Add redeem: Write fail");
        return false;
    }
    AddCache(hashBlock, ctxtRedeem);
    return true;
}

bool CRedeemDB::RemoveRedeem(const uint256& hashBlock)
{
    if (hashBlock == 0)
    {
        return true;
    }
    if (fCache)
    {
        if (IsFullRedeem(hashBlock))
        {
            mapCacheFullRedeem.erase(hashBlock);
        }
        else
        {
            mapCacheIncRedeem.erase(hashBlock);
        }
    }
    return Erase(hashBlock);
}

CRedeemContext* CRedeemDB::GetCacheRedeem(const uint256& hashBlock)
{
    if (hashBlock == 0)
    {
        return nullptr;
    }
    if (fCache)
    {
        if (IsFullRedeem(hashBlock))
        {
            auto it = mapCacheFullRedeem.find(hashBlock);
            if (it == mapCacheFullRedeem.end())
            {
                CRedeemContext ctxtRedeem;
                if (!Read(hashBlock, ctxtRedeem))
                {
                    StdError("CRedeemDB", "Get cache redeem: Read fail, block: %s", hashBlock.GetHex().c_str());
                    return nullptr;
                }
                AddCache(hashBlock, ctxtRedeem);
                it = mapCacheFullRedeem.find(hashBlock);
                if (it == mapCacheFullRedeem.end())
                {
                    StdError("CRedeemDB", "Get cache redeem: Find fail, block: %s", hashBlock.GetHex().c_str());
                    return nullptr;
                }
            }
            return &(it->second);
        }
        else
        {
            auto it = mapCacheIncRedeem.find(hashBlock);
            if (it == mapCacheIncRedeem.end())
            {
                CRedeemContext ctxtRedeem;
                if (!Read(hashBlock, ctxtRedeem))
                {
                    StdError("CRedeemDB", "Get cache redeem: Read fail, block: %s", hashBlock.GetHex().c_str());
                    return nullptr;
                }
                AddCache(hashBlock, ctxtRedeem);
                it = mapCacheIncRedeem.find(hashBlock);
                if (it == mapCacheIncRedeem.end())
                {
                    StdError("CRedeemDB", "Get cache redeem: Find fail, block: %s", hashBlock.GetHex().c_str());
                    return nullptr;
                }
            }
            return &(it->second);
        }
    }
    return nullptr;
}

bool CRedeemDB::GetRedeem(const uint256& hashBlock, CRedeemContext& ctxtRedeem)
{
    if (hashBlock == 0)
    {
        return true;
    }
    if (fCache)
    {
        CRedeemContext* pRedeem = GetCacheRedeem(hashBlock);
        if (pRedeem == nullptr)
        {
            StdError("CRedeemDB", "Get redeem: Get cache fail");
            return false;
        }
        ctxtRedeem = *pRedeem;
    }
    else
    {
        if (!Read(hashBlock, ctxtRedeem))
        {
            StdError("CRedeemDB", "Get redeem: Read fail");
            return false;
        }
    }
    return true;
}

bool CRedeemDB::GetRedeem(const uint256& hashBlock, map<CDestination, CDestRedeem>& mapRedeemOut)
{
    mapRedeemOut.clear();
    if (hashBlock == 0)
    {
        return true;
    }
    if (fCache)
    {
        CRedeemContext* pRedeem = GetCacheRedeem(hashBlock);
        if (pRedeem == nullptr)
        {
            StdError("CRedeemDB", "Get redeem: Get cache fail");
            return false;
        }
        mapRedeemOut = pRedeem->mapRedeem;
    }
    else
    {
        CRedeemContext ctxtRedeem;
        if (!Read(hashBlock, ctxtRedeem))
        {
            StdError("CRedeemDB", "Get redeem: Read fail");
            return false;
        }
        mapRedeemOut = ctxtRedeem.mapRedeem;
    }
    return true;
}

bool CRedeemDB::GetAddressRedeem(const uint256& hashBlock, const CDestination& dest, CDestRedeem& destRedeem)
{
    if (hashBlock == 0)
    {
        return true;
    }
    if (fCache)
    {
        CRedeemContext* pRedeem = GetCacheRedeem(hashBlock);
        if (pRedeem == nullptr)
        {
            StdError("CRedeemDB", "Get address redeem: Get cache fail");
            return false;
        }
        if (IsFullRedeem(hashBlock))
        {
            auto it = pRedeem->mapRedeem.find(dest);
            if (it != pRedeem->mapRedeem.end())
            {
                destRedeem = it->second;
            }
        }
        else
        {
            auto it = pRedeem->mapRedeem.find(dest);
            if (it != pRedeem->mapRedeem.end())
            {
                destRedeem = it->second;
            }
            else
            {
                if (pRedeem->hashRef != 0)
                {
                    CRedeemContext* pRefRedeem = GetCacheRedeem(pRedeem->hashRef);
                    if (pRefRedeem == nullptr)
                    {
                        StdError("CRedeemDB", "Get address redeem: Get ref cache fail");
                        return false;
                    }
                    auto mt = pRefRedeem->mapRedeem.find(dest);
                    if (mt != pRefRedeem->mapRedeem.end())
                    {
                        destRedeem = mt->second;
                    }
                }
            }
        }
    }
    else
    {
        CRedeemContext ctxtRedeem;
        if (!Read(hashBlock, ctxtRedeem))
        {
            StdError("CRedeemDB", "Get address redeem: Read fail");
            return false;
        }
        if (IsFullRedeem(hashBlock))
        {
            auto it = ctxtRedeem.mapRedeem.find(dest);
            if (it != ctxtRedeem.mapRedeem.end())
            {
                destRedeem = it->second;
            }
        }
        else
        {
            auto it = ctxtRedeem.mapRedeem.find(dest);
            if (it != ctxtRedeem.mapRedeem.end())
            {
                destRedeem = it->second;
            }
            else
            {
                if (ctxtRedeem.hashRef != 0)
                {
                    CRedeemContext ctxtRefRedeem;
                    if (!Read(ctxtRedeem.hashRef, ctxtRefRedeem))
                    {
                        StdError("CRedeemDB", "Get address redeem: Read fail");
                        return false;
                    }
                    auto mt = ctxtRefRedeem.mapRedeem.find(dest);
                    if (mt != ctxtRefRedeem.mapRedeem.end())
                    {
                        destRedeem = mt->second;
                    }
                }
            }
        }
    }
    return true;
}

void CRedeemDB::AddCache(const uint256& hashBlock, const CRedeemContext& ctxtRedeem)
{
    if (fCache)
    {
        if (IsFullRedeem(hashBlock))
        {
            auto it = mapCacheFullRedeem.find(hashBlock);
            if (it == mapCacheFullRedeem.end())
            {
                while (mapCacheFullRedeem.size() >= MAX_FULL_CACHE_COUNT)
                {
                    mapCacheFullRedeem.erase(mapCacheFullRedeem.begin());
                }
                mapCacheFullRedeem.insert(make_pair(hashBlock, ctxtRedeem));
            }
        }
        else
        {
            auto it = mapCacheIncRedeem.find(hashBlock);
            if (it == mapCacheIncRedeem.end())
            {
                while (mapCacheIncRedeem.size() >= MAX_INC_CACHE_COUNT)
                {
                    mapCacheIncRedeem.erase(mapCacheIncRedeem.begin());
                }
                mapCacheIncRedeem.insert(make_pair(hashBlock, ctxtRedeem));
            }
        }
    }
}

} // namespace storage
} // namespace minemon
