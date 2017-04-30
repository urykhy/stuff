#!/usr/bin/env python

#
# inspired by docker-dns/dockerdns and docker_ddns.rb
#
# dig docker axfr
# dig 19.172.in-addr.arpa axfr
#
# bit of bind9
# zone "16.172.in-addr.arpa" in { update-policy { grant "rndc-key" wildcard *;}; type master; file "master/16.172.in-addr.arpa"; };

import docker
import re
import json
from collections import namedtuple

import dns.query
import dns.tsigkeyring
import dns.update
import dns.rdatatype

Container = namedtuple('Container', 'id, name, running, addrs')
client = docker.Client("unix:///var/run/docker.sock", version='auto')
KEYRING = dns.tsigkeyring.from_text({'rndc-key' : 'oqLIg3VDxYIscfWapwwNSA=='})
ALGORITHM = dns.tsig.HMAC_MD5
DDNS_SERVER="127.0.0.1"

def get(d, *keys):
    empty = {}
    return reduce(lambda d, k: d.get(k, empty), keys, d) or None

def _get_addrs(networks):
    return [ value['IPAddress'] for value in networks.values() ]

def _get_names(name, labels):
    labels = labels or {}
    service = labels.get('com.docker.compose.service')
    project = labels.get('com.docker.compose.project')
    if project and project.endswith("docker"):
        project = project[:-6]
    if service > 0 and project > 0:
        name = '%s.%s' % (str(service), str(project))
    return [name]

def _inspect(cid):
    # get full details on this container from docker
    rec = client.inspect_container(cid)

    # ensure name is valid, and append our domain
    name = rec["Config"]["Hostname"]
    id = rec["Id"][:12]
    if name == id:
        name = get(rec, 'Name')[1:]
    if not name:
        return None

    id_ = get(rec, 'Id')
    labels = get(rec, 'Config', 'Labels')
    state = get(rec, 'State', 'Running')

    networks = get(rec, 'NetworkSettings', 'Networks')
    ip_addrs = _get_addrs(networks)

    return [ Container(id_, name, state, ip_addrs) for name in _get_names(name, labels) ]

def _update_ns(name, addr):
    print "updating forward zone %s:%s ..." % (name, addr)
    update = dns.update.Update("docker", keyring=KEYRING, keyalgorithm=ALGORITHM)
    update.delete(name, "A")
    update.add(name, 60, dns.rdatatype.A, str(addr))
    response = dns.query.tcp(update, DDNS_SERVER)
    if response.rcode() != 0:
        print "Failed: %s" % response

    rv = str(addr).split(".")
    rv.reverse()
    parts = ".".join(rv[:2])
    zone_name = ".".join(rv[2:])+".in-addr.arpa"
    print "updating rev zone %s for %s..." % (zone_name, name)
    update = dns.update.Update(zone_name, keyring=KEYRING, keyalgorithm=ALGORITHM)
    update.delete(parts, "PTR")
    update.add(parts, 60, dns.rdatatype.PTR, str(name)+".docker.")
    response = dns.query.tcp(update, DDNS_SERVER)
    if response.rcode() != 0:
        print "Failed: %s" % response

def _rm_ns(name, addr):
    print "delete from forward zone %s:%s ..." % (name, addr)
    update = dns.update.Update("docker", keyring=KEYRING, keyalgorithm=ALGORITHM)
    update.delete(name, "A")
    response = dns.query.tcp(update, DDNS_SERVER)
    if response.rcode() != 0:
        print "Failed: %s" % response

    rv = str(addr).split(".")
    rv.reverse()
    parts = ".".join(rv[:2])
    zone_name = ".".join(rv[2:])+".in-addr.arpa"
    print "delete from rev zone %s ..." % (zone_name)
    update = dns.update.Update(zone_name, keyring=KEYRING, keyalgorithm=ALGORITHM)
    update.delete(parts, "PTR")
    response = dns.query.tcp(update, DDNS_SERVER)
    if response.rcode() != 0:
        print "Failed: %s" % response

events = client.events()
for container in client.containers():
    for rec in _inspect(container['Id']):
        if rec.running:
            for addr in rec.addrs:
                _update_ns(rec.name, addr)

for raw in events:
    evt = json.loads(raw)
    if evt.get('Type', 'container') == 'container':
        cid = evt.get('id')
        if cid is None:
            continue
        status = evt.get('status')
        if status == 'start':
            for rec in _inspect(cid):
                for addr in rec.addrs:
                    for addr in rec.addrs:
                        _update_ns(rec.name, addr)
        if status == 'kill':
            for rec in _inspect(cid):
                for addr in rec.addrs:
                    for addr in rec.addrs:
                        _rm_ns(rec.name, addr)



