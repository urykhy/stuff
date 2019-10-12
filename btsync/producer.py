#!/usr/bin/env python3

import os
import sys
import time
import yaml
import libtorrent as lt
from loguru import logger

logger.debug("btsync producer starting")

with open("sync.yml", 'r') as stream:
    C = yaml.load(stream)

logger.debug(C)

torrentSet={}
state_str = ['queued', 'checking', 'downloading metadata', 'downloading', 'finished', 'seeding', 'allocating', 'checking fastresume']

def torrentName(filename):
    return C['producer']['bt']+"/"+filename+".torrent"

def createTorrent(filename):
    fs = lt.file_storage()
    lt.add_files(fs, C['producer']['source'] + "/" + filename)
    t = lt.create_torrent(fs)
    t.add_tracker(C['producer']['announce'], 0)
    t.set_creator('libtorrent %s' % lt.version)
    t.set_comment(C['producer']['comment'])
    lt.set_piece_hashes(t, C['producer']['source'])
    torrent = t.generate()
    tn = torrentName(filename)
    f = open(tn, "wb")
    f.write(lt.bencode(torrent))
    f.close()
    logger.debug("created torrent file for {}".format(filename))
    return tn

def addFile(filename, ses):
    global torrentSet
    tn = torrentName(filename)
    if not os.path.isfile(tn):
        createTorrent(filename)
    h = ses.add_torrent({'ti': lt.torrent_info(tn), 'save_path': C['producer']['source'], 'seed_mode': True})
    logger.debug("start seeding for {}".format(filename))
    torrentSet[filename]=h

logger.debug("create bt session")
cfg = {'alert_mask': lt.alert.category_t.status_notification
       | lt.alert.category_t.error_notification
       | lt.alert.category_t.storage_notification
       | lt.alert.category_t.dht_notification
       | lt.alert.category_t.tracker_notification
       | lt.alert.category_t.file_progress_notification,
       'enable_dht': False,
       'listen_interfaces': C['producer']['listen']
}
ses = lt.session(cfg)

while True:
    # check files and add em
    onlyfiles = [f for f in os.listdir(C['producer']['source']) if os.path.isfile(os.path.join(C['producer']['source'], f))]
    for f in onlyfiles:
        if not f in torrentSet:
            logger.info("new file {} found".format(f))
            addFile(f, ses)

    # print status
    alert = ses.pop_alert()
    while alert is not None:
        logger.debug("{}".format(alert.message()))
        alert = ses.pop_alert()

    # small delay
    time.sleep(1)