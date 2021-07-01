#!/usr/bin/env python

import time
import requests
import json
from collections import OrderedDict
import os
import sys
import random
from pprint import pprint
import pymysql
import random

COIN = 1000000
TX_FEE = 0.01

rpcurl = 'http://127.0.0.1:7704'

dbhost = 'localhost'
dbport = 3306
dbuser = "root"
dbpasswd = "qwe123"
dbname = "testdb"

genesis_privkey = '42b889a2668eda6d78682c23b5651fb76b5aac2b71caba1aa23b6b14d5ce75b7'
password = '123'

root_privkey = "c1e77fb9ac99dd516abbe9672823190922e4bf1005549788f08485fde175f5a9"
root_pubkey = "0ba7c9e89fcc3ad245e98b53ba6139871a34fa87a6a88ec828f3577fe904c156"
root_address = "1av0g9tbzazsjhj4en2k8fyhm3a3kjrdtae5yjhej7b69zt69mw5vxwq4"



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
        return 0
        #raise Exception('getbalance error: {}, addr: {}'.format(error, addr))


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
    #unlockkey(from_addr)

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

# RPC: sendfrom
def sendfrom_noalarm(from_addr, to, amount):
    #unlockkey(from_addr)

    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'sendfrom',
        'params': {
            'from': from_addr,
            'to': to,
            'amount': amount
        }
    })

    if result:
        txid = result
        # print('sendfrom success, txid: {}'.format(txid))
        return txid
    else:
        print('sendfrom fail, error: {}'.format(error))
        return "error"


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


# RPC: maketemplate mintpledge
def maketemplate_mintpledge(owner, powmint, rewardmode):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'maketemplate',
        'params': {
            'type': 'mintpledge',
            'mintpledge': {
                'owner': owner,
                'powmint': powmint,
                'rewardmode': rewardmode
            }
        }
    })

    if result:
        addr = result.get('address')
        # print('maketemplate success, template address: {}'.format(addr))
        return addr
    else:
        raise Exception('maketemplate mintpledge error: {}'.format(error))

# RPC: addnewtemplate mint
def addtemplate_mint(spent, pledgefee):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'addnewtemplate',
        'params': {
            'type': 'mint',
            'mint': {
                'spent': spent,
                'pledgefee': pledgefee
            }
        }
    })

    if result:
        addr = result
        # print('addnewtemplate success, template address: {}'.format(addr))
        return addr
    else:
        raise Exception('addnewtemplate mint error: {}'.format(error))

# RPC: addnewtemplate mintpledge
def addtemplate_mintpledge(owner, powmint, rewardmode):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'addnewtemplate',
        'params': {
            'type': 'mintpledge',
            'mintpledge': {
                'owner': owner,
                'powmint': powmint,
                'rewardmode': rewardmode
            }
        }
    })

    if result:
        addr = result
        # print('addnewtemplate success, template address: {}'.format(addr))
        return addr
    else:
        raise Exception('addnewtemplate mintpledge error: {}'.format(error))

# RPC: addnewtemplate mintredeem
def addtemplate_mintredeem(owner, nonce):
    result, error = call({
        'id': 1,
        'jsonrpc': '2.0',
        'method': 'addnewtemplate',
        'params': {
            'type': 'mintredeem',
            'mintredeem': {
                'owner': owner,
                'nonce': nonce
            }
        }
    })

    if result:
        addr = result
        # print('addnewtemplate success, template address: {}'.format(addr))
        return addr
    else:
        raise Exception('addnewtemplate mintredeem error: {}'.format(error))
        
        
##################################################################
def import_root_address():
    amount = getbalance_addr(root_address)
    if amount == 0:
        importprivkey(root_privkey)
    unlockkey(root_pubkey)
    return amount

def insert_userkey(address, prikey, pubkey, weight):
    conn = pymysql.connect(host = dbhost, port = dbport, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    sql="insert into userkey values(%s,%s,%s,%s)"
    cur.execute(sql, (address,prikey,pubkey,weight))

    cur.close()
    conn.commit()
    conn.close()
    #print('insert userkey success')

def rand_select_powaddress(pow_count):
    conn = pymysql.connect(host = dbhost, port = dbport, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    rand_value = random.randint(0,pow_count)
    powaddress = ""
    spentaddress = ""
    pledgefee = 0
    spentprivkey = ""
    
    cur.execute("select powaddress,spentaddress,pledgefee,spentprivkey from powmint;")
    while 1:
        res = cur.fetchone()
        if res is None:
            break
        powaddress = res[0]
        spentaddress = res[1]
        pledgefee = res[2]
        spentprivkey = res[3]

        rand_value = rand_value - 1
        if rand_value <= 0:
            break

    cur.close()
    conn.commit()
    conn.close()
    return  powaddress, spentaddress, pledgefee, spentprivkey

def query_powaddress_count():
    conn = pymysql.connect(host = dbhost, port = dbport, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    pow_count = 0
    
    cur.execute("select count(*) from powmint;")
    while 1:
        res = cur.fetchone()
        if res is None:
            break
        pow_count = res[0]

    cur.close()
    conn.commit()
    conn.close()
    return  pow_count

def query_vote_count():
    conn = pymysql.connect(host = dbhost, port = dbport, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    count = 0
    
    cur.execute("select count(*) from vote;")
    while 1:
        res = cur.fetchone()
        if res is None:
            break
        count = res[0]

    cur.close()
    conn.commit()
    conn.close()
    return  count

def rand_import_powaddress():
    pow_count = query_powaddress_count()
    powaddress, spentaddress, pledgefee, spentprivkey = rand_select_powaddress(pow_count)
    importprivkey(spentprivkey)
    unlockkey(spentaddress)
    pow_address = addtemplate_mint(spentaddress, pledgefee)
    return pow_address

def select_rand_powaddress():
    pow_count = query_powaddress_count()
    powaddress, spentaddress, pledgefee, spentprivkey = rand_select_powaddress(pow_count)
    return powaddress

def insert_vote_address(voteaddr, owner, powmint, ownerprivkey, amount):
    conn = pymysql.connect(host = dbhost, port = dbport, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    now = int(time.time())
    rand_value = random.randint(30,1800)

    sql="insert into vote values(%s,%s,%s,%s,%s,%s,%s,%s)"
    cur.execute(sql, (voteaddr, owner, powmint, 1, ownerprivkey, amount, now+rand_value, 0))

    cur.close()
    conn.commit()
    conn.close()

def update_vote_nextredeemtime(voteaddr):
    conn = pymysql.connect(host = dbhost, port = dbport, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    now = int(time.time())
    redeem_rand_value = random.randint(180,900)
    vote_rand_value = random.randint(30,1800)

    sql="update vote set nextredeemtime=%s,nextvotetime=%s where voteaddress=%s"
    cur.execute(sql, (now+redeem_rand_value+vote_rand_value, now+redeem_rand_value, voteaddr))

    cur.close()
    conn.commit()
    conn.close()

def update_vote_nextvotetime(voteaddr):
    conn = pymysql.connect(host = dbhost, port = dbport, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    now = int(time.time())
    redeem_rand_value = random.randint(180,900)

    sql="update vote set nextvotetime=%s where voteaddress=%s"
    cur.execute(sql, (now+redeem_rand_value, voteaddr))

    cur.close()
    conn.commit()
    conn.close()

def user_create():
    pubkey, privkey = makekeypair()
    address = getpubkeyaddress(pubkey)
    insert_userkey(address, privkey, pubkey, random.randint(0,10000)) #random.randint(0,10000)
    importprivkey(privkey)
    unlockkey(address)
    print('create user success, address: {}'.format(address))
    return address, privkey

def user_vote(address, privkey, powaddress, balance, vote_amount):
    voteaddr = addtemplate_mintpledge(address, powaddress, 1)
    unlockkey(root_address)
    sendfrom(root_address, voteaddr, vote_amount)
    insert_vote_address(voteaddr, address, powaddress, privkey, vote_amount)
    print('user vote success, vote amount: {}, vote address: {}, balance: {}, pow address: {}'.format(vote_amount, voteaddr, balance-vote_amount-0.01, powaddress))

def user_rand_vote(count):
    import_root_address()
    for i in range(0, count):
        amount = getbalance_addr(root_address)
        vote_amount = random.randint(100,300)
        if amount >= vote_amount+0.01:
            powaddress = select_rand_powaddress()
            user_address, privkey = user_create()
            user_vote(user_address, privkey, powaddress, amount, vote_amount)

##############################################
def trans_pow_amount():
    import_root_address()

    conn = pymysql.connect(host = dbhost, port = dbport, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    cur.execute("select powaddress,spentaddress,pledgefee,spentprivkey from powmint;")
    while 1:
        res = cur.fetchone()
        if res is None:
            break
        powaddress = res[0]
        spentaddress = res[1]
        pledgefee = res[2]
        spentprivkey = res[3]

        pow_address = addtemplate_mint(spentaddress, pledgefee)
        amount = getbalance_addr(pow_address)
        #print('trans amount: pow: {}, banalce: {}'.format(pow_address, amount))
        if amount > 0.01:
            importprivkey(spentprivkey)
            unlockkey(spentaddress)
            txid = sendfrom_noalarm(pow_address, root_address, -1)
            if txid == "error":
                print('trans pow amount: sendfrom fail, pow: {}, amount: {}'.format(pow_address, amount))
            else:
                print('trans pow amount: sendfrom success, pow: {}, amount: {}'.format(pow_address, amount))

    cur.close()
    conn.commit()
    conn.close()

##############################################
def update_vote_amountt(voteaddress, amount):
    conn = pymysql.connect(host = dbhost, port = dbport, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    sql="update vote set amount=%s where voteaddress=%s;"
    cur.execute(sql, (amount,voteaddress))

    cur.close()
    conn.commit()
    conn.close()
    
def save_vote_amount():
    conn = pymysql.connect(host = dbhost, port = dbport, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    vote_count = 0

    cur.execute("select voteaddress,owner from vote;")
    while 1:
        res = cur.fetchone()
        if res is None:
            break
        voteaddress = res[0]
        owner = res[1]

        amount = getbalance_addr(voteaddress)
        update_vote_amountt(voteaddress, amount)
        vote_count += 1
        #print('vote amount: voteaddress: {}, amount: {}, count: {}'.format(voteaddress, amount, vote_count))

    cur.close()
    conn.commit()
    conn.close()
    return vote_count

##############################################
def vote_redeem():
    conn = pymysql.connect(host = dbhost, port = dbport, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    redeem_count = 0

    cur.execute("select voteaddress,owner,ownerprivkey,nextredeemtime from vote;")
    while 1:
        res = cur.fetchone()
        if res is None:
            break
        voteaddress = res[0]
        owner = res[1]
        ownerprivkey = res[2]
        nextredeemtime = res[3]

        now = int(time.time())
        if now > nextredeemtime:
            update_vote_nextredeemtime(voteaddress)
            amount = getbalance_addr(voteaddress)
            if amount > 0.01:
                redeemaddress = addtemplate_mintredeem(owner, 1)
                importprivkey(ownerprivkey)
                unlockkey(owner)
                txid = sendfrom_noalarm(voteaddress, redeemaddress, -1)
                redeem_count += 1
                if txid == "error":
                    print('user redeem fail: voteaddress: {}, amount: {}'.format(voteaddress, amount))
                else:
                    print('user redeem success: voteaddress: {}, amount: {}'.format(voteaddress, amount))

    cur.close()
    conn.commit()
    conn.close()
    return redeem_count

##############################################
def trans_redeem_to_vote():
    conn = pymysql.connect(host = dbhost, port = dbport, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    trans_count = 0

    cur.execute("select voteaddress,owner,ownerprivkey,nextvotetime from vote;")
    while 1:
        res = cur.fetchone()
        if res is None:
            break
        voteaddress = res[0]
        owner = res[1]
        ownerprivkey = res[2]
        nextvotetime = res[3]

        now = int(time.time())
        if nextvotetime > 0 and now > nextvotetime:
            update_vote_nextvotetime(voteaddress)
            redeemaddress = addtemplate_mintredeem(owner, 1)
            amount = getbalance_addr(redeemaddress)
            if amount > 0.01:
                importprivkey(ownerprivkey)
                unlockkey(owner)
                txid = sendfrom_noalarm(redeemaddress, voteaddress, -1)
                trans_count += 1
                if txid == "error":
                    print('trans redeem to vote fail: voteaddress: {}, amount: {}'.format(voteaddress, amount))
                else:
                    print('trans redeem to vote success: voteaddress: {}, amount: {}'.format(voteaddress, amount))

    cur.close()
    conn.commit()
    conn.close()
    return trans_count

##############################################
def timer_create_user():
    user_count = 0
    create_user_time = 0
    redeem_time = 0
    vote_redeem_time = 0
    save_vote_time = 0
    rand_user_time = 0
    sleep_len = 2
    create_user_flag = True

    while user_count < 100000:
        time.sleep(sleep_len)

        trans_pow_amount()

        create_user_time += sleep_len
        if create_user_flag and create_user_time >= rand_user_time:
            create_user_time = 0
            vote_user_count = query_vote_count()
            if vote_user_count >= 800:
                create_user_flag = False
                print('create user too more, vote count: {}'.format(vote_user_count))
            else:
                user_rand_vote(1)
                user_count = user_count + 1
                rand_user_time = random.randint(1,20)
                print('create user success, count: {}, next wait: {}'.format(user_count, rand_user_time))

        redeem_time += sleep_len
        if redeem_time >= 15:
            redeem_time = 0
            count = vote_redeem()
            print('redeem success: {} **********************************'.format(count))

        vote_redeem_time += sleep_len
        if vote_redeem_time >= 10:
            vote_redeem_time = 0
            count = trans_redeem_to_vote()
            print('redeem to vote success: {} ............................'.format(count))

        save_vote_time += sleep_len
        if save_vote_time >= 53:
            save_vote_time = 0
            vote_count = save_vote_amount()
            print('save vote amount success: {} ###########################'.format(vote_count))


##################################################################
if __name__ == "__main__":
    workmode = "non"

    if len(sys.argv) < 3:
        raise Exception('argv error, count: {}'.format(len(sys.argv)))

    path = os.path.join(os.getcwd(), sys.argv[1])

    input = {}
    
    # load json
    with open(path, 'r') as r:
        content = json.loads(r.read())
        input = content["input"]

    rpcurl = 'http://'+input['rpc']['host']+':'+str(input['rpc']['port'])
    
    dbhost = input['db']['dbhost']
    dbport = input['db']['dbport']
    dbuser = input['db']['dbuser']
    dbpasswd = input['db']['dbpwd']
    dbname = input['db']['dbname']

    workmode = sys.argv[2]

    #######################################
    if workmode == "user_rand_vote":
        count = 1
        if len(sys.argv) >= 4:
            count = int(sys.argv[3])
        user_rand_vote(count)

    elif workmode == "trans_pow_amount":
        trans_pow_amount()

    elif workmode == "save_vote_amount":
        save_vote_amount()

    elif workmode == "vote_redeem":
        vote_redeem()

    elif workmode == "trans_redeem_to_vote":
        trans_redeem_to_vote()

    elif workmode == "timer_create_user":
        timer_create_user()

