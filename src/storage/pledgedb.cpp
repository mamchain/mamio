// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pledgedb.h"

#include <boost/range/adaptor/reversed.hpp>

#include "leveldbeng.h"

#include "block.h"
#include "param.h"

using namespace std;
using namespace xengine;

namespace minemon
{
namespace storage
{

//////////////////////////////
// CPledgeDB

bool CPledgeDB::Initialize(const boost::filesystem::path& pathData)
{
    CLevelDBArguments args;
    args.path = (pathData / "pledge").string();
    args.syncwrite = false;
    CLevelDBEngine* engine = new CLevelDBEngine(args);

    if (!Open(engine))
    {
        delete engine;
        return false;
    }
    return true;
}

void CPledgeDB::Deinitialize()
{
    mapCacheFullPledge.clear();
    mapCacheIncPledge.clear();
    Close();
}

bool CPledgeDB::AddNew(const uint256& hashBlock, const CPledgeContext& ctxtPledge)
{
    xengine::CWriteLock wlock(rwData);
    return AddPledge(hashBlock, ctxtPledge);
}

bool CPledgeDB::Remove(const uint256& hashBlock)
{
    xengine::CWriteLock wlock(rwData);
    return RemovePledge(hashBlock);
}

bool CPledgeDB::RetrieveFullPledge(const uint256& hashBlock, CPledgeContext& ctxtPledge)
{
    xengine::CReadLock rlock(rwData);
    return GetFullPledge(hashBlock, ctxtPledge);
}

bool CPledgeDB::AddBlockPledge(const uint256& hashBlock, const uint256& hashPrev, const std::map<CDestination, std::map<CDestination, std::pair<int64, int>>>& mapBlockPledgeIn)
{
    xengine::CWriteLock wlock(rwData);

    CPledgeContext ctxtPledge;
    if (IsFullPledge(hashBlock))
    {
        if (!GetFullBlockPledge(hashBlock, hashPrev, mapBlockPledgeIn, ctxtPledge))
        {
            StdError("CPledgeDB", "Add block pledge: Add full pledge fail");
            return false;
        }
    }
    else
    {
        if (!GetIncrementBlockPledge(hashBlock, hashPrev, mapBlockPledgeIn, ctxtPledge))
        {
            StdError("CPledgeDB", "Add block pledge: Add increment pledge fail");
            return false;
        }
    }

    if (!AddPledge(hashBlock, ctxtPledge))
    {
        StdError("CPledgeDB", "Add block pledge: Add pledge fail");
        return false;
    }
    return true;
}

bool CPledgeDB::GetBlockPledge(const uint256& hashBlock, const uint256& hashPrev, const std::map<CDestination, std::map<CDestination, std::pair<int64, int>>>& mapBlockPledgeIn, CPledgeContext& ctxtPledge)
{
    xengine::CReadLock rlock(rwData);
    if (IsFullPledge(hashBlock))
    {
        if (!GetFullBlockPledge(hashBlock, hashPrev, mapBlockPledgeIn, ctxtPledge))
        {
            return false;
        }
    }
    else
    {
        if (!GetIncrementBlockPledge(hashBlock, hashPrev, mapBlockPledgeIn, ctxtPledge))
        {
            return false;
        }
    }
    return true;
}

bool CPledgeDB::RetrieveBlockPledge(const uint256& hashBlock, CPledgeContext& ctxtPledge)
{
    xengine::CReadLock rlock(rwData);
    return GetPledge(hashBlock, ctxtPledge);
}

bool CPledgeDB::RetrievePowPledgeList(const uint256& hashBlock, const CDestination& destPowMint, std::map<CDestination, std::pair<int64, int>>& mapPowPledgeList)
{
    xengine::CReadLock rlock(rwData);
    return GetPowPledgeList(hashBlock, destPowMint, mapPowPledgeList);
}

bool CPledgeDB::RetrieveAddressPledgeData(const uint256& hashBlock, const CDestination& destPowMint, const CDestination& destPledge, int64& nPledgeAmount, int& nPledgeHeight)
{
    xengine::CReadLock rlock(rwData);
    return GetAddressPledgeData(hashBlock, destPowMint, destPledge, nPledgeAmount, nPledgeHeight);
}

void CPledgeDB::Clear()
{
    mapCacheFullPledge.clear();
    mapCacheIncPledge.clear();
    RemoveAll();
}

///////////////////////////////////////////////
bool CPledgeDB::IsFullPledge(const uint256& hashBlock)
{
    if (CBlock::GetBlockHeightByHash(hashBlock) == 0)
    {
        return true;
    }
    return ((CBlock::GetBlockHeightByHash(hashBlock) % BPX_PLEDGE_REWARD_DISTRIBUTE_HEIGHT) == 1);
}

bool CPledgeDB::GetFullBlockPledge(const uint256& hashBlock, const uint256& hashPrev, const std::map<CDestination, std::map<CDestination, std::pair<int64, int>>>& mapBlockPledgeIn, CPledgeContext& ctxtPledge)
{
    if (!GetFullPledge(hashPrev, ctxtPledge))
    {
        StdError("CPledgeDB", "Get full: Get pledge fail");
        return false;
    }
    ctxtPledge.AddIncPledge(mapBlockPledgeIn);
    ctxtPledge.ClearEmpty();
    return true;
}

bool CPledgeDB::GetIncrementBlockPledge(const uint256& hashBlock, const uint256& hashPrev, const std::map<CDestination, std::map<CDestination, std::pair<int64, int>>>& mapBlockPledgeIn, CPledgeContext& ctxtPledge)
{
    if (!GetPledge(hashPrev, ctxtPledge))
    {
        StdError("CPledgeDB", "Get increment: Get pledge fail");
        return false;
    }
    if (ctxtPledge.IsFull())
    {
        ctxtPledge.hashRef = hashPrev;
        ctxtPledge.mapPledge.clear();
    }
    ctxtPledge.AddIncPledge(mapBlockPledgeIn);
    return true;
}

bool CPledgeDB::GetFullPledge(const uint256& hashBlock, CPledgeContext& ctxtPledge)
{
    if (!GetPledge(hashBlock, ctxtPledge))
    {
        StdError("CPledgeDB", "Get full pledge: Get pledge fail");
        return false;
    }
    if (!ctxtPledge.IsFull())
    {
        std::map<CDestination, std::map<CDestination, std::pair<int64, int>>> mapIncPledge;
        mapIncPledge = ctxtPledge.mapPledge;
        ctxtPledge.mapPledge.clear();
        if (!GetPledge(ctxtPledge.hashRef, ctxtPledge.mapPledge))
        {
            StdError("CPledgeDB", "Get full pledge: Get pledge fail, ref: %s", ctxtPledge.hashRef.GetHex().c_str());
            return false;
        }
        ctxtPledge.AddIncPledge(mapIncPledge);
        ctxtPledge.hashRef = 0;
    }
    return true;
}

bool CPledgeDB::AddPledge(const uint256& hashBlock, const CPledgeContext& ctxtPledge)
{
    if (!Write(hashBlock, ctxtPledge))
    {
        StdError("CPledgeDB", "Add pledge: Write fail");
        return false;
    }
    AddCache(hashBlock, ctxtPledge);
    return true;
}

bool CPledgeDB::RemovePledge(const uint256& hashBlock)
{
    if (hashBlock == 0)
    {
        return true;
    }
    if (fCache)
    {
        if (IsFullPledge(hashBlock))
        {
            mapCacheFullPledge.erase(hashBlock);
        }
        else
        {
            mapCacheIncPledge.erase(hashBlock);
        }
    }
    return Erase(hashBlock);
}

CPledgeContext* CPledgeDB::GetCachePledgeContext(const uint256& hashBlock)
{
    if (hashBlock == 0)
    {
        return nullptr;
    }
    if (fCache)
    {
        if (IsFullPledge(hashBlock))
        {
            auto it = mapCacheFullPledge.find(hashBlock);
            if (it == mapCacheFullPledge.end())
            {
                CPledgeContext ctxtPledge;
                if (!Read(hashBlock, ctxtPledge))
                {
                    StdError("CPledgeDB", "Get cache pledge: Read fail, block: %s", hashBlock.GetHex().c_str());
                    return nullptr;
                }
                AddCache(hashBlock, ctxtPledge);
                it = mapCacheFullPledge.find(hashBlock);
                if (it == mapCacheFullPledge.end())
                {
                    StdError("CPledgeDB", "Get cache pledge: Find fail, block: %s", hashBlock.GetHex().c_str());
                    return nullptr;
                }
            }
            return &(it->second);
        }
        else
        {
            auto it = mapCacheIncPledge.find(hashBlock);
            if (it == mapCacheIncPledge.end())
            {
                CPledgeContext ctxtPledge;
                if (!Read(hashBlock, ctxtPledge))
                {
                    StdError("CPledgeDB", "Get cache pledge: Read fail, block: %s", hashBlock.GetHex().c_str());
                    return nullptr;
                }
                AddCache(hashBlock, ctxtPledge);
                it = mapCacheIncPledge.find(hashBlock);
                if (it == mapCacheIncPledge.end())
                {
                    StdError("CPledgeDB", "Get cache pledge: Find fail, block: %s", hashBlock.GetHex().c_str());
                    return nullptr;
                }
            }
            return &(it->second);
        }
    }
    return nullptr;
}

bool CPledgeDB::GetPledge(const uint256& hashBlock, CPledgeContext& ctxtPledge)
{
    if (hashBlock == 0)
    {
        return true;
    }
    if (fCache)
    {
        CPledgeContext* pCachePledge = GetCachePledgeContext(hashBlock);
        if (pCachePledge == nullptr)
        {
            return false;
        }
        ctxtPledge = *pCachePledge;
    }
    else
    {
        if (!Read(hashBlock, ctxtPledge))
        {
            StdError("CPledgeDB", "Get pledge: Read fail");
            return false;
        }
    }
    return true;
}

bool CPledgeDB::GetPledge(const uint256& hashBlock, std::map<CDestination, std::map<CDestination, std::pair<int64, int>>>& mapPledgeOut)
{
    mapPledgeOut.clear();
    if (hashBlock == 0)
    {
        return true;
    }
    if (fCache)
    {
        CPledgeContext* pCachePledge = GetCachePledgeContext(hashBlock);
        if (pCachePledge == nullptr)
        {
            return false;
        }
        mapPledgeOut = pCachePledge->mapPledge;
    }
    else
    {
        CPledgeContext ctxtPledge;
        if (!Read(hashBlock, ctxtPledge))
        {
            StdError("CPledgeDB", "Get pledge: Read fail");
            return false;
        }
        mapPledgeOut = ctxtPledge.mapPledge;
    }
    return true;
}

bool CPledgeDB::GetPowPledgeList(const uint256& hashBlock, const CDestination& destPowMint, std::map<CDestination, std::pair<int64, int>>& mapPowPledgeList)
{
    if (hashBlock == 0)
    {
        return true;
    }
    if (fCache)
    {
        CPledgeContext* pCachePledge = GetCachePledgeContext(hashBlock);
        if (pCachePledge == nullptr)
        {
            StdError("CPledgeDB", "Get pow pledge: Get cache fail, block: %s", hashBlock.GetHex().c_str());
            return false;
        }
        if (IsFullPledge(hashBlock))
        {
            auto it = pCachePledge->mapPledge.find(destPowMint);
            if (it != pCachePledge->mapPledge.end())
            {
                mapPowPledgeList = it->second;
            }
        }
        else
        {
            if (pCachePledge->hashRef != 0)
            {
                CPledgeContext* pCacheRefPledge = GetCachePledgeContext(pCachePledge->hashRef);
                if (pCacheRefPledge == nullptr)
                {
                    StdError("CPledgeDB", "Get pow pledge: Get ref cache fail, ref block: %s, block: %s",
                             pCachePledge->hashRef.GetHex().c_str(), hashBlock.GetHex().c_str());
                    return false;
                }
                auto it = pCacheRefPledge->mapPledge.find(destPowMint);
                if (it != pCacheRefPledge->mapPledge.end())
                {
                    mapPowPledgeList = it->second;
                }
            }
            auto nt = pCachePledge->mapPledge.find(destPowMint);
            if (nt != pCachePledge->mapPledge.end())
            {
                for (const auto& kv : nt->second)
                {
                    auto& powPledge = mapPowPledgeList[kv.first];
                    powPledge.first += kv.second.first;
                    if (kv.second.second > 0)
                    {
                        powPledge.second = kv.second.second;
                    }
                }
            }
        }
    }
    else
    {
        CPledgeContext ctxtPledge;
        if (!GetFullPledge(hashBlock, ctxtPledge))
        {
            StdError("CPledgeDB", "Get pow pledge: Get full pledge fail, block: %s", hashBlock.GetHex().c_str());
            return false;
        }
        auto it = ctxtPledge.mapPledge.find(destPowMint);
        if (it != ctxtPledge.mapPledge.end())
        {
            mapPowPledgeList = it->second;
        }
    }
    return true;
}

bool CPledgeDB::GetAddressPledgeData(const uint256& hashBlock, const CDestination& destPowMint, const CDestination& destPledge, int64& nPledgeAmount, int& nPledgeHeight)
{
    nPledgeAmount = 0;
    nPledgeHeight = 0;
    if (hashBlock == 0)
    {
        return true;
    }
    if (fCache)
    {
        CPledgeContext* pCachePledge = GetCachePledgeContext(hashBlock);
        if (pCachePledge == nullptr)
        {
            StdError("CPledgeDB", "Get address pledge: Get cache fail, block: %s", hashBlock.GetHex().c_str());
            return false;
        }
        if (IsFullPledge(hashBlock))
        {
            auto it = pCachePledge->mapPledge.find(destPowMint);
            if (it != pCachePledge->mapPledge.end())
            {
                auto mt = it->second.find(destPledge);
                if (mt != it->second.end())
                {
                    nPledgeAmount = mt->second.first;
                    nPledgeHeight = mt->second.second;
                }
            }
        }
        else
        {
            if (pCachePledge->hashRef != 0)
            {
                CPledgeContext* pCacheRefPledge = GetCachePledgeContext(pCachePledge->hashRef);
                if (pCacheRefPledge == nullptr)
                {
                    StdError("CPledgeDB", "Get address pledge: Get ref cache fail, ref block: %s, block: %s",
                             pCachePledge->hashRef.GetHex().c_str(), hashBlock.GetHex().c_str());
                    return false;
                }
                auto nt = pCacheRefPledge->mapPledge.find(destPowMint);
                if (nt != pCacheRefPledge->mapPledge.end())
                {
                    auto mt = nt->second.find(destPledge);
                    if (mt != nt->second.end())
                    {
                        nPledgeAmount = mt->second.first;
                        nPledgeHeight = mt->second.second;
                    }
                }
            }
            auto kt = pCachePledge->mapPledge.find(destPowMint);
            if (kt != pCachePledge->mapPledge.end())
            {
                auto mt = kt->second.find(destPledge);
                if (mt != kt->second.end())
                {
                    nPledgeAmount += mt->second.first;
                    if (mt->second.second > 0)
                    {
                        nPledgeHeight = mt->second.second;
                    }
                }
            }
        }
    }
    else
    {
        CPledgeContext ctxtPledge;
        if (!GetFullPledge(hashBlock, ctxtPledge))
        {
            StdError("CPledgeDB", "Get address pledge: Get full pledge fail, block: %s", hashBlock.GetHex().c_str());
            return false;
        }
        auto it = ctxtPledge.mapPledge.find(destPowMint);
        if (it != ctxtPledge.mapPledge.end())
        {
            auto mt = it->second.find(destPledge);
            if (mt != it->second.end())
            {
                nPledgeAmount = mt->second.first;
                nPledgeHeight = mt->second.second;
            }
        }
    }
    return true;
}

void CPledgeDB::AddCache(const uint256& hashBlock, const CPledgeContext& ctxtPledge)
{
    if (fCache)
    {
        if (IsFullPledge(hashBlock))
        {
            auto it = mapCacheFullPledge.find(hashBlock);
            if (it == mapCacheFullPledge.end())
            {
                while (mapCacheFullPledge.size() >= MAX_FULL_CACHE_COUNT)
                {
                    mapCacheFullPledge.erase(mapCacheFullPledge.begin());
                }
                mapCacheFullPledge.insert(make_pair(hashBlock, ctxtPledge));
            }
        }
        else
        {
            auto it = mapCacheIncPledge.find(hashBlock);
            if (it == mapCacheIncPledge.end())
            {
                while (mapCacheIncPledge.size() >= MAX_INC_CACHE_COUNT)
                {
                    mapCacheIncPledge.erase(mapCacheIncPledge.begin());
                }
                mapCacheIncPledge.insert(make_pair(hashBlock, ctxtPledge));
            }
        }
    }
}

} // namespace storage
} // namespace minemon
