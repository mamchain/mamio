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

bool CPledgeDB::Retrieve(const uint256& hashBlock, CPledgeContext& ctxtPledge)
{
    xengine::CReadLock rlock(rwData);
    if (!GetPledge(hashBlock, ctxtPledge))
    {
        StdError("CPledgeDB", "Retrieve: Get pledge fail, block: %s", hashBlock.GetHex().c_str());
        return false;
    }
    if (!ctxtPledge.IsFull())
    {
        if (!GetPledge(ctxtPledge.hashRef, ctxtPledge.mapPledge))
        {
            StdError("CPledgeDB", "Retrieve: Get pledge fail, ref: %s", ctxtPledge.hashRef.GetHex().c_str());
            return false;
        }
        for (const auto& kv : ctxtPledge.mapBlockPledge)
        {
            ctxtPledge.mapPledge[kv.second.first][kv.first] += kv.second.second;
        }
        ctxtPledge.hashRef = 0;
        ctxtPledge.mapBlockPledge.clear();
    }
    return true;
}

bool CPledgeDB::AddBlockPledge(const uint256& hashBlock, const uint256& hashPrev, const map<CDestination, pair<CDestination, int64>>& mapBlockPledgeIn)
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

bool CPledgeDB::GetBlockPledge(const uint256& hashBlock, const uint256& hashPrev, const std::map<CDestination, std::pair<CDestination, int64>>& mapBlockPledgeIn, CPledgeContext& ctxtPledge)
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

void CPledgeDB::Clear()
{
    mapCacheFullPledge.clear();
    mapCacheIncPledge.clear();
    RemoveAll();
}

///////////////////////////////////////////////
bool CPledgeDB::IsFullPledge(const uint256& hashBlock)
{
    return ((CBlock::GetBlockHeightByHash(hashBlock) % BPX_PLEDGE_REWARD_DISTRIBUTE_HEIGHT) == 1);
}

bool CPledgeDB::GetFullBlockPledge(const uint256& hashBlock, const uint256& hashPrev, const map<CDestination, pair<CDestination, int64>>& mapBlockPledgeIn, CPledgeContext& ctxtPledge)
{
    if (!GetPledge(hashPrev, ctxtPledge))
    {
        StdError("CPledgeDB", "Get full: Get pledge fail");
        return false;
    }

    if (!ctxtPledge.IsFull())
    {
        if (!GetPledge(ctxtPledge.hashRef, ctxtPledge.mapPledge))
        {
            StdError("CPledgeDB", "Get full: Get pledge fail, ref: %s", ctxtPledge.hashRef.GetHex().c_str());
            return false;
        }
        for (const auto& kv : ctxtPledge.mapBlockPledge)
        {
            ctxtPledge.mapPledge[kv.second.first][kv.first] += kv.second.second;
        }
    }
    ctxtPledge.hashRef = 0;
    ctxtPledge.mapBlockPledge.clear();

    for (const auto& kv : mapBlockPledgeIn)
    {
        ctxtPledge.mapPledge[kv.second.first][kv.first] += kv.second.second;
    }
    ctxtPledge.ClearEmpty();
    return true;
}

bool CPledgeDB::GetIncrementBlockPledge(const uint256& hashBlock, const uint256& hashPrev, const map<CDestination, pair<CDestination, int64>>& mapBlockPledgeIn, CPledgeContext& ctxtPledge)
{
    if (!GetPledge(hashPrev, ctxtPledge))
    {
        StdError("CPledgeDB", "Get increment: Get pledge fail");
        return false;
    }

    for (const auto& kv : mapBlockPledgeIn)
    {
        auto& destPledge = ctxtPledge.mapBlockPledge[kv.first];
        destPledge.first = kv.second.first;
        destPledge.second += kv.second.second;
    }
    /*for (auto it = ctxtPledge.mapBlockPledge.begin(); it != ctxtPledge.mapBlockPledge.end();)
    {
        if (it->second.second <= 0)
        {
            ctxtPledge.mapBlockPledge.erase(it++);
        }
        else
        {
            ++it;
        }
    }*/

    if (ctxtPledge.IsFull())
    {
        ctxtPledge.hashRef = hashPrev;
        ctxtPledge.mapPledge.clear();
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

bool CPledgeDB::GetPledge(const uint256& hashBlock, CPledgeContext& ctxtPledge)
{
    if (hashBlock == 0)
    {
        return true;
    }
    if (fCache)
    {
        if (IsFullPledge(hashBlock))
        {
            auto it = mapCacheFullPledge.find(hashBlock);
            if (it == mapCacheFullPledge.end())
            {
                if (!Read(hashBlock, ctxtPledge))
                {
                    StdError("CPledgeDB", "Get pledge: Read fail");
                    return false;
                }
                AddCache(hashBlock, ctxtPledge);
            }
            else
            {
                ctxtPledge = it->second;
            }
        }
        else
        {
            auto it = mapCacheIncPledge.find(hashBlock);
            if (it == mapCacheIncPledge.end())
            {
                if (!Read(hashBlock, ctxtPledge))
                {
                    StdError("CPledgeDB", "Get pledge: Read fail");
                    return false;
                }
                AddCache(hashBlock, ctxtPledge);
            }
            else
            {
                ctxtPledge = it->second;
            }
        }
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

bool CPledgeDB::GetPledge(const uint256& hashBlock, map<CDestination, map<CDestination, int64>>& mapPledgeOut)
{
    mapPledgeOut.clear();
    if (hashBlock == 0)
    {
        return true;
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
                    StdError("CPledgeDB", "Get pledge: Read fail");
                    return false;
                }
                AddCache(hashBlock, ctxtPledge);
                mapPledgeOut = ctxtPledge.mapPledge;
            }
            else
            {
                mapPledgeOut = it->second.mapPledge;
            }
        }
        else
        {
            auto it = mapCacheIncPledge.find(hashBlock);
            if (it == mapCacheIncPledge.end())
            {
                CPledgeContext ctxtPledge;
                if (!Read(hashBlock, ctxtPledge))
                {
                    StdError("CPledgeDB", "Get pledge: Read fail");
                    return false;
                }
                AddCache(hashBlock, ctxtPledge);
                mapPledgeOut = ctxtPledge.mapPledge;
            }
            else
            {
                mapPledgeOut = it->second.mapPledge;
            }
        }
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
