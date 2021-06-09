// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pledgedb.h"

#include <boost/range/adaptor/reversed.hpp>

#include "leveldbeng.h"

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
    cachePledge.Clear();
    Close();
}

bool CPledgeDB::AddNew(const uint256& hashBlock, const CPledgeContext& ctxtPledge)
{
    xengine::CWriteLock wlock(rwData);
    if (!Write(hashBlock, ctxtPledge))
    {
        StdError("CPledgeDB", "AddNew Write fail");
        return false;
    }
    if (fCache)
    {
        cachePledge.AddNew(hashBlock, ctxtPledge);
    }
    return true;
}

bool CPledgeDB::Remove(const uint256& hashBlock)
{
    xengine::CWriteLock wlock(rwData);
    if (fCache)
    {
        cachePledge.Remove(hashBlock);
    }
    return Erase(hashBlock);
}

bool CPledgeDB::Retrieve(const uint256& hashBlock, CPledgeContext& ctxtPledge)
{
    xengine::CReadLock rlock(rwData);
    if (fCache)
    {
        if (cachePledge.Retrieve(hashBlock, ctxtPledge))
        {
            return true;
        }
        if (!Read(hashBlock, ctxtPledge))
        {
            StdError("CPledgeDB", "Retrieve Read fail");
            return false;
        }
        cachePledge.AddNew(hashBlock, ctxtPledge);
    }
    else
    {
        if (!Read(hashBlock, ctxtPledge))
        {
            StdError("CPledgeDB", "Retrieve Read fail");
            return false;
        }
    }
    return true;
}

void CPledgeDB::Clear()
{
    cachePledge.Clear();
    RemoveAll();
}

} // namespace storage
} // namespace minemon
