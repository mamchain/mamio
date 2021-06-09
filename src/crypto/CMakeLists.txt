#------------------------------------------------------------------------------
# CMake file for Minemon
#
# Copyright (c) 2019-2021 The Minemon developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#------------------------------------------------------------------------------

set(sources
    uint256.h
    crc24q.cpp      crc24q.h
    base32.cpp      base32.h
    crypto.cpp      crypto.h
    key.cpp         key.h
    keystore.cpp    keystore.h
)

add_library(crypto ${sources})

include_directories(../xengine ../common ./ ${sodium_INCLUDE_DIR})

target_link_libraries(crypto
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    OpenSSL::SSL
    OpenSSL::Crypto
    ${sodium_LIBRARY_RELEASE}
    xengine
    common
)