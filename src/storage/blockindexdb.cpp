// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockindexdb.h"

#include "leveldbeng.h"

using namespace std;
using namespace xengine;
#if BOOST_VERSION >= 106000
using namespace boost::placeholders;
#endif

namespace minemon
{
namespace storage
{

//////////////////////////////
// CBlockIndexDB

bool CBlockIndexDB::Initialize(const boost::filesystem::path& pathData)
{
    CLevelDBArguments args;
    args.path = (pathData / "blockindex").string();
    args.syncwrite = false;
    CLevelDBEngine* engine = new CLevelDBEngine(args);

    if (!Open(engine))
    {
        delete engine;
        return false;
    }

    return true;
}

void CBlockIndexDB::Deinitialize()
{
    Close();
}

bool CBlockIndexDB::AddNewBlock(const CBlockOutline& outline)
{
    return Write(outline.GetBlockHash(), outline);
}

bool CBlockIndexDB::RemoveBlock(const uint256& hashBlock)
{
    return Erase(hashBlock);
}

bool CBlockIndexDB::RetrieveBlock(const uint256& hashBlock, CBlockOutline& outline)
{
    return Read(hashBlock, outline);
}

bool CBlockIndexDB::WalkThroughBlock(CBlockDBWalker& walker)
{
    return WalkThrough(boost::bind(&CBlockIndexDB::LoadBlockWalker, this, _1, _2, boost::ref(walker)));
}

void CBlockIndexDB::Clear()
{
    RemoveAll();
}

bool CBlockIndexDB::LoadBlockWalker(CBufStream& ssKey, CBufStream& ssValue, CBlockDBWalker& walker)
{
    CBlockOutline outline;
    ssValue >> outline;
    return walker.Walk(outline);
}

} // namespace storage
} // namespace minemon
