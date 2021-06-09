// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "redeemdb.h"

#include <boost/range/adaptor/reversed.hpp>

#include "leveldbeng.h"

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
    cacheRedeem.Clear();
    Close();
}

bool CRedeemDB::AddNew(const uint256& hashBlock, const CRedeemContext& redeemContext)
{
    xengine::CWriteLock wlock(rwData);
    if (!Write(hashBlock, redeemContext))
    {
        return false;
    }
    if (fCache)
    {
        cacheRedeem.AddNew(hashBlock, redeemContext);
    }
    return true;
}

bool CRedeemDB::Remove(const uint256& hashBlock)
{
    xengine::CWriteLock wlock(rwData);
    if (fCache)
    {
        cacheRedeem.Remove(hashBlock);
    }
    return Erase(hashBlock);
}

bool CRedeemDB::Retrieve(const uint256& hashBlock, CRedeemContext& redeemContext)
{
    xengine::CReadLock rlock(rwData);
    if (fCache)
    {
        if (cacheRedeem.Retrieve(hashBlock, redeemContext))
        {
            return true;
        }
        if (!Read(hashBlock, redeemContext))
        {
            return false;
        }
        cacheRedeem.AddNew(hashBlock, redeemContext);
    }
    else
    {
        if (!Read(hashBlock, redeemContext))
        {
            return false;
        }
    }
    return true;
}

void CRedeemDB::Clear()
{
    cacheRedeem.Clear();
    RemoveAll();
}

} // namespace storage
} // namespace minemon
