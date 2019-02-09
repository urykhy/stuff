#!/usr/bin/env python3

# apt install python3-yaml
# pip3 install elasticsearch elasticsearch_dsl

import re
import sys
import yaml
import pickle
import smtplib
from email.message import EmailMessage
import datetime
import os.path

import logging
logging.basicConfig(format='%(asctime)s %(levelname)s: %(message)s', level=logging.ERROR)

with open("esalert.yaml", 'r') as stream:
    C = yaml.load(stream)

from elasticsearch import Elasticsearch
from elasticsearch_dsl import Search, Q

logging.info("start for {}".format(C["config"]['server']))
es = Elasticsearch([C["config"]['server']])

state = {}
now = datetime.datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%S.000Z')
#now = "now-1d"

def readState():
    global state
    if not os.path.isfile(C["config"]["state"]):
        logging.info("no state file")
        return
    try:
        with open(C["config"]["state"], 'rb') as f:
            state = pickle.load(f)
            logging.info("state recovered")
    except:
        logging.error("state error: {}".format(sys.exc_info()[0]))

def saveState():
    with open(C["config"]["state"], 'wb') as f:
        pickle.dump(state, f)

def sendMail(s):
    CE = C["email"]
    msg = EmailMessage()
    msg.set_content(s)
    msg['Subject'] = '[ESalert] digest'
    msg['From'] = CE['from']
    msg['To'] = CE['to']
    s = smtplib.SMTP(CE['server'])
    s.send_message(msg)
    s.quit()

def matchType(k, v):
    kw = {k: v}
    if '?' in v or '*' in v:
        return Q("wildcard", **kw)
    else:
        return Q("match", **kw)

readState()

if 'lastrun' in state:
    logging.info("last run is {}".format(state['lastrun']))

mbody = ""
for x in C["rules"]:
    logging.info("evaludate rule {}".format(x))
    s = Search(using=es, index=C["config"]['index'])
    q = None
    regexp = None
    for r in C["rules"][x]:
        for (k,v) in r.items():
            if k == 'regexp':
                regexp = v
                continue
            logging.debug("add {}: {}".format(k, v))
            if q is None:
                q = matchType(k, v)
            else:
                q = q & matchType(k, v)
    s = s.query(q)
    if 'lastrun' in state:
        s = s.filter("range", ** {'@timestamp':{'gte' : state['lastrun'] }})
    s = s.sort('-@timestamp')
    s = s.extra(size=C["config"]['size'])
    r = s.execute()
    history = {}
    empty = True
    body = "Last messages for {} (max {})\n\n".format(x, C["config"]['size'])
    for x in r:
        if regexp is not None and re.match(regexp, x.message) is None:
            continue
        k = "{}:{}".format(x.logsource, x.message.strip())
        if not k in history:
            s = "{}| {}: {}".format(x.timestamp, x.logsource, x.message.strip())
            body += s
            body += "\n"
            logging.info("result: {}".format(s))
            history[k]=1
            empty = False
    body += "\n\n"
    if not empty:
        mbody += body
if len(mbody) > 0:
    sendMail(mbody)
state['lastrun'] = now
saveState()

