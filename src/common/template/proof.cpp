// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "proof.h"

#include "key.h"
#include "rpc/auto_protocol.h"
#include "template.h"
#include "util.h"

using namespace std;
using namespace xengine;
using namespace minemon::crypto;

//////////////////////////////
// CTemplateProof

CTemplateProof::CTemplateProof(const CDestination& destSpentIn, const uint32 nPledgeFeeIn)
  : CTemplate(TEMPLATE_PROOF), destSpent(destSpentIn), nPledgeFee(nPledgeFeeIn)
{
}

CTemplateProof* CTemplateProof::clone() const
{
    return new CTemplateProof(*this);
}

bool CTemplateProof::GetSignDestination(const CTransaction& tx, const uint256& hashFork, int nHeight, const std::vector<uint8>& vchSig,
                                        std::set<CDestination>& setSubDest, std::vector<uint8>& vchSubSig) const
{
    if (!CTemplate::GetSignDestination(tx, hashFork, nHeight, vchSig, setSubDest, vchSubSig))
    {
        return false;
    }

    setSubDest.clear();
    setSubDest.insert(destSpent);
    return true;
}

void CTemplateProof::GetTemplateData(minemon::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.mint.strSpent = (destInstance = destSpent).ToString();
    obj.mint.nPledgefee = nPledgeFee;
}

bool CTemplateProof::ValidateParam() const
{
    if (!IsTxSpendable(destSpent))
    {
        return false;
    }
    if (nPledgeFee > 1000)
    {
        return false;
    }
    return true;
}

bool CTemplateProof::SetTemplateData(const vector<uint8>& vchDataIn)
{
    CIDataStream is(vchDataIn);
    try
    {
        is >> destSpent >> nPledgeFee;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }

    return true;
}

bool CTemplateProof::SetTemplateData(const minemon::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_PROOF))
    {
        return false;
    }

    if (!destInstance.ParseString(obj.mint.strSpent))
    {
        return false;
    }
    destSpent = destInstance;

    nPledgeFee = obj.mint.nPledgefee;
    return true;
}

void CTemplateProof::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << destSpent << nPledgeFee;
}

bool CTemplateProof::VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                       const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const
{
    return destSpent.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
}

bool CTemplateProof::VerifyBlockSignature(const uint256& hash, const vector<uint8>& vchSig) const
{
    return false;
}
