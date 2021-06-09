// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mintpledge.h"

#include "destination.h"
#include "rpc/auto_protocol.h"
#include "template.h"
#include "transaction.h"
#include "util.h"

using namespace std;
using namespace xengine;

//////////////////////////////
// CTemplateMintPledge

CTemplateMintPledge::CTemplateMintPledge(const CDestination& destOwnerIn, const CDestination& destPowMintIn, int nRewardModeIn)
  : CTemplate(TEMPLATE_MINTPLEDGE),
    destOwner(destOwnerIn),
    destPowMint(destPowMintIn),
    nRewardMode(nRewardModeIn)
{
}

CTemplateMintPledge* CTemplateMintPledge::clone() const
{
    return new CTemplateMintPledge(*this);
}

bool CTemplateMintPledge::GetSignDestination(const CTransaction& tx, const uint256& hashFork, int nHeight, const vector<uint8>& vchSig,
                                             set<CDestination>& setSubDest, vector<uint8>& vchSubSig) const
{
    if (!CTemplate::GetSignDestination(tx, hashFork, nHeight, vchSig, setSubDest, vchSubSig))
    {
        return false;
    }
    setSubDest.clear();
    setSubDest.insert(destOwner);
    return true;
}

void CTemplateMintPledge::GetTemplateData(minemon::rpc::CTemplateResponse& obj, CDestination&& destInstance) const
{
    obj.mintpledge.strOwner = (destInstance = destOwner).ToString();
    obj.mintpledge.strPowmint = (destInstance = destPowMint).ToString();
    obj.mintpledge.nRewardmode = nRewardMode;
}

bool CTemplateMintPledge::ValidateParam() const
{
    if (!IsTxSpendable(destOwner))
    {
        return false;
    }
    if (!destPowMint.IsTemplate() || destPowMint.GetTemplateId().GetType() != TEMPLATE_PROOF)
    {
        return false;
    }
    if (nRewardMode != 1 && nRewardMode != 2)
    {
        return false;
    }
    return true;
}

bool CTemplateMintPledge::SetTemplateData(const std::vector<uint8>& vchDataIn)
{
    CIDataStream is(vchDataIn);
    try
    {
        is >> destOwner >> destPowMint >> nRewardMode;
    }
    catch (exception& e)
    {
        StdError(__PRETTY_FUNCTION__, e.what());
        return false;
    }
    return true;
}

bool CTemplateMintPledge::SetTemplateData(const minemon::rpc::CTemplateRequest& obj, CDestination&& destInstance)
{
    if (obj.strType != GetTypeName(TEMPLATE_MINTPLEDGE))
    {
        return false;
    }

    if (!destInstance.ParseString(obj.mintpledge.strOwner))
    {
        return false;
    }
    destOwner = destInstance;

    if (!destInstance.ParseString(obj.mintpledge.strPowmint))
    {
        return false;
    }
    destPowMint = destInstance;

    nRewardMode = obj.mintpledge.nRewardmode;
    return true;
}

void CTemplateMintPledge::BuildTemplateData()
{
    vchData.clear();
    CODataStream os(vchData);
    os << destOwner << destPowMint << nRewardMode;
}

bool CTemplateMintPledge::VerifyTxSignature(const uint256& hash, const uint16 nType, const uint256& hashAnchor, const CDestination& destTo,
                                            const vector<uint8>& vchSig, const int32 nForkHeight, bool& fCompleted) const
{
    return destOwner.VerifyTxSignature(hash, nType, hashAnchor, destTo, vchSig, nForkHeight, fCompleted);
}
