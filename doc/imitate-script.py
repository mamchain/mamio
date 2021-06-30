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

rpcurl = 'http://127.0.0.1:7702'

dbhost = 'localhost'
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
        

########################################3
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

def create_table():
    conn = pymysql.connect('localhost', user = "root", passwd = "qwe123", db = "testdb")
    cursor = conn.cursor()
    print(cursor)

    cursor.execute('drop table if exists user')
    sql="""CREATE TABLE IF NOT EXISTS `user` (
        `id` int(11) NOT NULL AUTO_INCREMENT,
        `name` varchar(255) NOT NULL,
        `age` int(11) NOT NULL,
        PRIMARY KEY (`id`)
        ) ENGINE=InnoDB  DEFAULT CHARSET=utf8 AUTO_INCREMENT=0"""

    cursor.execute(sql)
    cursor.close()
    conn.close()
    print('Create table success')

def insert_data():
    conn=pymysql.connect('localhost','root','qwe123')
    conn.select_db('testdb')

    cur=conn.cursor()

    cur.execute('drop table if exists user')
    sql="""CREATE TABLE IF NOT EXISTS `user` (
        `id` int(11) NOT NULL AUTO_INCREMENT,
        `name` varchar(255) NOT NULL,
        `age` int(11) NOT NULL,
        PRIMARY KEY (`id`)
        ) ENGINE=InnoDB  DEFAULT CHARSET=utf8 AUTO_INCREMENT=0"""

    cur.execute(sql)

    insert=cur.execute("insert into user values(1,'tom',18)")
    print('insert lines:',insert)

    sql="insert into user values(%s,%s,%s)"
    cur.execute(sql,(3,'kongsh',20))

    cur.close()
    conn.commit()
    conn.close()
    print('sql success')

def select_data():
    conn=pymysql.connect('localhost','root','qwe123')
    conn.select_db('testdb')
    cur=conn.cursor()

    cur.execute("select * from user;")
    while 1:
        res=cur.fetchone()
        if res is None:
            break
        print (res)
    cur.close()
    conn.commit()
    conn.close()
    print('sql success')


def create_table_userkey():
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
    cursor = conn.cursor()
    print(cursor)

    #cursor.execute('drop table if exists user')
    sql = """CREATE TABLE IF NOT EXISTS `userkey` (
        `address` varchar(128) NOT NULL,
        `privkey` varchar(128) DEFAULT NULL,
        `pubkey` varchar(128) DEFAULT NULL,
        `weight` int(11) DEFAULT '0',
        PRIMARY KEY (`address`)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8;""";

    cursor.execute(sql)
    cursor.close()
    conn.close()
    print('Create table success')


def insert_userkey(address, prikey, pubkey, weight):
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    sql="insert into userkey values(%s,%s,%s,%s)"
    cur.execute(sql, (address,prikey,pubkey,weight))

    cur.close()
    conn.commit()
    conn.close()
    print('insert userkey success')


def create_userkey_data(count):
    create_table_userkey()

    for i in range(0, count):
        pubkey, privkey = makekeypair()
        address = getpubkeyaddress(pubkey)
        print('i: {}, {}, {}, {}'.format(i, address, privkey, pubkey))
        insert_userkey(address, privkey, pubkey, i)

def clear_userkey_data():
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    sql="delete from userkey;"
    cur.execute(sql)

    cur.close()
    conn.commit()
    conn.close()
    print('clear userkey success')

def userkey_import():
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    cur.execute("select address,privkey from userkey;")
    while 1:
        res=cur.fetchone()
        if res is None:
            break
        print ('address: {}, privkey: {}'.format(res[0], res[1]))
        importprivkey(res[1])
        unlockkey(res[0])
        #print (res)
    cur.close()
    conn.commit()
    conn.close()
    print('import userkey success')

def userkey_sendtoken():
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    from_address = "1j6x8vdkkbnxe8qwjggfan9c8m8zhmez7gm3pznsqxgch3eyrwxby8eda"
    unlockkey(from_address)

    cur.execute("select address from userkey;")
    while 1:
        res=cur.fetchone()
        if res is None:
            break
        sendfrom(from_address, res[0], 200)
        print ('sendfrom address: {}'.format(res[0]))
        #print (res)
    cur.close()
    conn.commit()
    conn.close()
    print('userkey send token success')

def userkey_create_vote_address():
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    powmint = "20g0fr64202t8r8tkxw782z7nrq9h1h1frznyctnveksdzxvc2rnn2v52"
    
    cur.execute("select address from userkey;")
    while 1:
        res=cur.fetchone()
        if res is None:
            break
        owner = res[0]
        voteaddr = maketemplate_mintpledge(owner, powmint, 1)
        print ('vote address: {}'.format(voteaddr))

        cur_insert = conn.cursor()
        sql="insert into vote values(%s,%s,%s,%s,%s)"
        cur_insert.execute(sql, (voteaddr,owner,powmint,1,0))
        cur_insert.close()
        #print (res)

    cur.close()
    conn.commit()
    conn.close()

def userkey_vote():
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()
    
    from_address = "1j6x8vdkkbnxe8qwjggfan9c8m8zhmez7gm3pznsqxgch3eyrwxby8eda"
    unlockkey(from_address)

    cur.execute("select owner,powmint,rewardmode from vote;")
    while 1:
        res=cur.fetchone()
        if res is None:
            break
        print ('data: {},{},{}'.format(res[0],res[1],res[2]))
        voteaddr = addtemplate_mintpledge(res[0],res[1],res[2])
        print ('vote address: {}'.format(voteaddr))
        sendfrom(from_address, voteaddr, 200)

        #print (res)

    cur.close()
    conn.commit()
    conn.close()
    print('userkey vote success')

def userkey_vote2():
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()
    
    cur.execute("select owner,powmint,rewardmode from vote;")
    while 1:
        res=cur.fetchone()
        if res is None:
            break
        print ('data: {},{},{}'.format(res[0],res[1],res[2]))
        voteaddr = addtemplate_mintpledge(res[0],res[1],res[2])
        print ('vote address: {}'.format(voteaddr))
        amount = getbalance_addr(res[0])
        unlockkey(res[0])
        sendfrom(res[0], voteaddr, amount-0.01)
        print ('send success; {}'.format(res[0]))

        #print (res)

    cur.close()
    conn.commit()
    conn.close()
    print('userkey vote success')

def userkey_redeem():
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()
    
    from_address = "1j6x8vdkkbnxe8qwjggfan9c8m8zhmez7gm3pznsqxgch3eyrwxby8eda"
    unlockkey(from_address)

    cur.execute("select voteaddress,owner from vote;")
    while 1:
        res=cur.fetchone()
        if res is None:
            break
        voteaddress = res[0]
        owner = res[1]
        redeemaddr = addtemplate_mintredeem(owner,1)
        amount = getbalance_addr(voteaddress)
        unlockkey(owner)
        sendfrom(voteaddress, redeemaddr, amount-0.01)
        print ('send success: vote: {}, redeem: {}'.format(voteaddress, redeemaddr))

        #print (res)

    cur.close()
    conn.commit()
    conn.close()
    print('userkey redeem success')


##################################################################
def import_root_address():
    amount = getbalance_addr(root_address)
    if amount == 0:
        importprivkey(root_privkey)
    unlockkey(root_pubkey)
    return amount

def rand_select_powaddress(pow_count):
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
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
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
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
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    now = int(time.time())
    rand_value = random.randint(30,1800)

    sql="insert into vote values(%s,%s,%s,%s,%s,%s,%s,%s)"
    cur.execute(sql, (voteaddr, owner, powmint, 1, ownerprivkey, amount, now+rand_value, 0))

    cur.close()
    conn.commit()
    conn.close()

def update_vote_nextredeemtime(voteaddr):
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
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
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
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

    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
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
        print('trans amount: pow: {}, banalce: {}'.format(pow_address, amount))
        if amount > 1:
            importprivkey(spentprivkey)
            unlockkey(spentaddress)
            sendfrom(pow_address, root_address, amount-1)
            print('trans amount: pow: {}, amount: {}'.format(pow_address, amount-1))

    cur.close()
    conn.commit()
    conn.close()

##############################################
def update_vote_amountt(voteaddress, amount):
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
    cur = conn.cursor()

    sql="update vote set amount=%s where voteaddress=%s;"
    cur.execute(sql, (amount,voteaddress))

    cur.close()
    conn.commit()
    conn.close()
    
def save_vote_amount():
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
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
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
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
                sendfrom(voteaddress, redeemaddress, amount-0.01)
                redeem_count += 1
                print('vote redeem: voteaddress: {}, amount: {}'.format(voteaddress, amount))

    cur.close()
    conn.commit()
    conn.close()
    return redeem_count

##############################################
def trans_redeem_to_vote():
    conn = pymysql.connect(dbhost, user = dbuser, passwd = dbpasswd, db = dbname)
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
                sendfrom(redeemaddress, voteaddress, amount-0.01)
                trans_count += 1
                print('trans redeem: voteaddress: {}, amount: {}'.format(voteaddress, amount))

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
    sleep_len =2
    while user_count < 100000:
        time.sleep(sleep_len)

        create_user_time += sleep_len
        if create_user_time >= rand_user_time:
            create_user_time = 0
            trans_pow_amount()
            user_rand_vote(1)
            user_count = user_count + 1
            rand_user_time = random.randint(1,20)
            print('create user success, count: {}, wait: {}'.format(user_count, rand_user_time))

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

    if len(sys.argv) < 2:
        raise Exception('argv error, count: {}'.format(len(sys.argv)))

    workmode = sys.argv[1]

    if workmode == "userkey_create":
        count = 1
        if len(sys.argv) >= 3:
            count = int(sys.argv[2])
        create_userkey_data(count)

    elif workmode == "userkey_clear":
        clear_userkey_data()

    elif workmode == "userkey_import":
        userkey_import()

    elif workmode == "userkey_sendtoken":
        userkey_sendtoken()

    elif workmode == "userkey_createvote":
        userkey_create_vote_address()

    elif workmode == "userkey_vote":
        userkey_vote()

    elif workmode == "userkey_vote2":
        userkey_vote2()

    elif workmode == "userkey_redeem":
        userkey_redeem()

    #######################################
    elif workmode == "user_rand_vote":
        count = 1
        if len(sys.argv) >= 3:
            count = int(sys.argv[2])
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

