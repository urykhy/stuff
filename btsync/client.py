#!/usr/bin/env python3

import os
import sys
import time
import yaml
import libtorrent as lt
from loguru import logger

logger.debug("btsync client starting")

with open("sync.yml", 'r') as stream:
    C = yaml.load(stream)

torrentSet={}
state_str = ['queued', 'checking', 'downloading metadata', 'downloading', 'finished', 'seeding', 'allocating', 'checking fastresume']

def addFile(filename, ses):
    global torrentSet
    h = ses.add_torrent({'ti': lt.torrent_info(C['client']['bt']+"/"+filename), 'save_path': C['client']['dest']})
    logger.debug("downloading {}".format(filename))
    torrentSet[filename]=h

logger.debug("create bt session")
cfg = {'alert_mask': lt.alert.category_t.status_notification
       | lt.alert.category_t.error_notification
       | lt.alert.category_t.storage_notification
       | lt.alert.category_t.dht_notification
       | lt.alert.category_t.tracker_notification
       | lt.alert.category_t.file_progress_notification,
       'enable_dht': False,
       'listen_interfaces': C['client']['listen']
}
ses = lt.session(cfg)

while True:
    # check files and add em
    onlyfiles = [f for f in os.listdir(C['client']['bt']) if os.path.isfile(os.path.join(C['client']['bt'], f))]
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
