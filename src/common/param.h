// Copyright (c) 2019-2021 The Minemon developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MINEMON_PARAM_H
#define MINEMON_PARAM_H

#include "uint256.h"

extern bool TESTNET_FLAG;
#define GET_PARAM(MAINNET_PARAM, TESTNET_PARAM) (TESTNET_FLAG ? TESTNET_PARAM : MAINNET_PARAM)

static const int64 COIN = 1000000;
static const int64 CENT = 10000;
static const int64 MIN_TX_FEE = CENT;
static const int64 MAX_MONEY = 1000000000000 * COIN;
inline bool MoneyRange(int64 nValue)
{
    return (nValue >= 0 && nValue <= MAX_MONEY);
}
static const int64 MAX_REWARD_MONEY = 10000 * COIN;
inline bool RewardRange(int64 nValue)
{
    return (nValue >= 0 && nValue <= MAX_REWARD_MONEY);
}
inline double ValueFromCoin(uint64 amount)
{
    return ((double)amount / (double)COIN);
}

static const unsigned int MAX_BLOCK_SIZE = 2000000;
static const unsigned int MAX_TX_SIZE = (MAX_BLOCK_SIZE / 20);
static const unsigned int MAX_SIGNATURE_SIZE = 2048;
static const unsigned int MAX_TX_INPUT_COUNT = (MAX_TX_SIZE - MAX_SIGNATURE_SIZE - 4) / 33;
static const unsigned int BLOCK_TARGET_SPACING = 60; // 1-minute block spacing

enum ConsensusMethod
{
    CM_SHA256D = 0,
    CM_MAX
};

static const int64 MAX_CLOCK_DRIFT = 80;

#define PROOF_OF_WORK_BITS_LOWER_LIMIT GET_PARAM(8, 8) //GET_PARAM(45, 8)
static const int PROOF_OF_WORK_BITS_UPPER_LIMIT = 200;
#define PROOF_OF_WORK_BITS_INIT GET_PARAM(20, 20) //GET_PARAM(50, 20)
#define PROOF_OF_WORK_DIFFICULTY_INTERVAL GET_PARAM(2880, 2880)

static const int64 BPX_INIT_REWARD_TOKEN = 0 * COIN;
static const int64 BPX_INIT_BLOCK_REWARD_TOKEN = 100 * COIN;
static const int64 BPX_REWARD_HALVE_HEIGHT = 60 * 24 * 365 * 2;
static const int64 BPX_POW_REWARD_RATE = 382;
static const int64 BPX_TOTAL_REWARD_RATE = 1000;
static const int64 BPX_MIN_PLEDGE_COIN = 100 * COIN;

#define BPX_REPEAT_MINT_HEIGHT GET_PARAM(1, 1)               //GET_PARAM(100, 1)
#define BPX_PLEDGE_REWARD_DISTRIBUTE_HEIGHT GET_PARAM(10, 5) //GET_PARAM(1440, 5)
#define BPX_REDEEM_DAY_COUNT GET_PARAM(10, 10)               //GET_PARAM(100, 10)
#define BPX_REDEEM_DAY_HEIGHT GET_PARAM(10, 5)               //GET_PARAM(1440, 5)

#endif //MINEMON_PARAM_H
