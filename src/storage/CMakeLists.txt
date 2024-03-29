#------------------------------------------------------------------------------
# CMake file for Minemon
#
# Copyright (c) 2019-2021 The Minemon developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#------------------------------------------------------------------------------

set(sources
    timeseries.cpp      timeseries.h
    blockdb.cpp         blockdb.h
    blockbase.cpp       blockbase.h
    blockindexdb.cpp    blockindexdb.h
    walletdb.cpp        walletdb.h
    txpooldata.cpp      txpooldata.h
    unspentdb.cpp       unspentdb.h
    forkdb.cpp          forkdb.h
    templatedatadb.cpp  templatedatadb.h
    pledgedb.cpp        pledgedb.h
    redeemdb.cpp        redeemdb.h
    purger.cpp          purger.h
    leveldbeng.cpp      leveldbeng.h
    txindexdb.cpp       txindexdb.h
    ctsdb.cpp           ctsdb.h
)

add_library(storage ${sources})

include_directories(../xengine ../crypto ../common ../leveldb/include ../snappy ../delegate ../mpvss)

target_link_libraries(storage
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_TIMER_LIBRARY}
    ${Boost_CHRONO_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    OpenSSL::SSL
    OpenSSL::Crypto
    xengine
    crypto
    common
    leveldb    
    snappy
   # delegate
   # mpvss
)
