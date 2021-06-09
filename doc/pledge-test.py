#!/usr/bin/env python

import time
import requests
import json
from collections import OrderedDict
import os
import sys
import random
from pprint import pprint

COIN = 1000000
TX_FEE = 0.01

rpcurl = 'http://127.0.0.1:7702'
rpcurl_1 = 'http://127.0.0.1:5567'
rpcurl_2 = 'http://127.0.0.1:5578'

genesis_privkey = '42b889a2668eda6d78682c23b5651fb76b5aac2b71caba1aa23b6b14d5ce75b7'

# RPC HTTP request
def call(body):
    req = requests.post(rpcurl, json=body)

    #    print('DEBUG: request: {}'.format(body))
    #    print('DEBUG: response: {}'.format(req.content))

    resp = json.loads(req.content.decode('utf-8'))
    return resp.get('result'), resp.get('error')

# RPC HTTP request
def call_url(rurl, body):
    req = requests.post(rurl, json=body)
    resp = json.loads(req.content.decode('utf-8'))
    return resp.get('result'), resp.get('error')


# RPC: makekeypair
def makekeypair():
    result, error = call({
        'id': 0,
        'jsonrpc': '1.0',
        'method': 'makekeypair',
        'params': {}
    })

    if result:
        pubkey = result.get('pubkey')
        privkey = result.get('privkey')
        # print('makekeypair success, pubkey: {}'.format(pubkey))
        return pubkey, privkey
    else:
        raise Exception('makekeypair error: {}'.format(error))


# RPC: getnewkey
def getnewkey():
    result, error = call({
        'id': 0,
        'jsonrpc': '1.0',
        'method': 'getnewkey',
        'params': {
            'passphrase': password
        }
    })

    if result:
        pubkey = result
        # print('getnewkey success, pubkey: {}'.format(pubkey))
        return pubkey
    else:
        raise Exception('getnewkey error: {}'.format(error))


# RPC: getpubkeyaddress
def getpubkeyaddress(pubkey):
    result, error = call({
        'id': 0,
        'jsonrpc': '1.0',
        'method': 'getpubkeyaddress',
        'params': {
            "pubkey": pubkey
        }
    })

    if result:
        address = result
        # print('getpubkeyaddress success, address: {}'.format(address))
        return address
    else:
        raise Exception('getpubkeyaddress error: {}'.format(error))


# RPC: importprivkey
def importprivkey(privkey, synctx=True):
    result, error = call({
        'id': 0,
        'jsonrpc': '1.0',
        'method': 'importprivkey',
        'params': {
            'privkey': privkey,
            'passphrase': password,
            'synctx': synctx
        }
    })

    if result:
        pubkey = result
        # print('importprivkey success, pubkey: {}'.format(pubkey))
        return pubkey
    else:
        raise Exception('importprivkey error: {}'.format(error))


# RPC: getbalance
def getbalance():
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'getbalance',
        'params': {
        }
    })

    if result and len(result) > 0:
        print('getbalance success, length: {}'.format(len(result)))
        avail = result[0].get('avail')
        # print('getbalance success, avail: {}'.format(avail))
        return avail
    else:
        raise Exception('getbalance error: {}'.format(error))

# RPC: stat balance
def statbalance(url):
    result, error = call_url(url, {
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'getbalance',
        'params': {
        }
    })

    if result and len(result) > 0:
        print('getbalance success, length: {}'.format(len(result)))
        avail = 0
        for i in range(0, len(result)):
            avail += result[i].get('avail')
        return avail
    else:
        raise Exception('getbalance error: {}'.format(error))

# RPC: getbalance addr
def getbalance_addr(addr):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'getbalance',
        'params': {
            'address': addr
        }
    })

    if result and len(result) == 1:
        avail = result[0].get('avail')
        # print('getbalance success, avail: {}'.format(avail))
        return avail
    else:
        raise Exception('getbalance error: {}'.format(error))


# RPC: unlockkey
def unlockkey(key):
    call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'unlockkey',
        'params': {
            'pubkey': key,
            'passphrase': password
        }
    })


# RPC: sendfrom
def sendfrom(from_addr, to, amount, fork=None, type=0, data=None):
    unlockkey(from_addr)

    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'sendfrom',
        'params': {
            'from': from_addr,
            'to': to,
            'amount': amount,
            'fork': fork,
            'type': type,
            'data': data
        }
    })

    if result:
        txid = result
        # print('sendfrom success, txid: {}'.format(txid))
        return txid
    else:
        raise Exception('sendfrom error: {}'.format(error))


# RPC: getforkheight
def getforkheight(forkid=None):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'getforkheight',
        'params': {
            'fork': forkid,
        }
    })

    if result:
        height = result
        # print('getforkheight success, height: {}'.format(height))
        return height
    else:
        return None


# RPC: getblockhash
def getblockhash(height, forkid=None):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'getblockhash',
        'params': {
            'height': height,
            'fork': forkid,
        }
    })

    if result:
        block_hash = result
        # print('getblockhash success, block hash: {}'.format(block_hash))
        return block_hash
    else:
        return None


# RPC: getblock
def getblock(blockid):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'getblock',
        'params': {
            'block': blockid,
        }
    })

    if result:
        block = result
        # print('getblock success, block: {}'.format(block))
        return block
    else:
        raise Exception('getblock error: {}'.format(error))


# RPC: getblockdetail
def getblockdetail(blockid):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'getblockdetail',
        'params': {
            'block': blockid,
        }
    })

    if result:
        block = result
        # print('getblockdetail success, block: {}'.format(block))
        return block
    else:
        raise Exception('getblockdetail error: {}'.format(error))


# RPC: gettransaction
def gettransaction(txid):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'gettransaction',
        'params': {
            'txid': txid,
        }
    })

    if result:
        tx = result['transaction']
        # print('gettransaction success, tx: {}'.format(tx))
        return tx
    else:
        raise Exception('gettransaction error: {}'.format(error))



def checkbalance():
    while True:
        height=getforkheight()
        print('height: {}'.format(height))

        if height % 5 == 1:
            b0 = statbalance(rpcurl)
            print('balance 0: {}'.format(b0))
            b1 = statbalance(rpcurl_1)
            print('balance 1: {}'.format(b1))
            b2 = statbalance(rpcurl_2)
            print('balance 2: {}'.format(b2))

            stat = b0+b1+b2+61.8
            print('stat: {}, total: {}'.format(stat, height * 100))
            break

        time.sleep(1)

if __name__ == "__main__":
    checkbalance()
