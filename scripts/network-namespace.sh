#!/bin/bash

NAME=protonvpn

ip netns add  $NAME
ip netns exec $NAME ip addr add 127.0.0.1/8 dev lo
ip netns exec $NAME ip link set lo up

# space - это интерфейс для выхода из NS в мир
ip link add space0 type veth peer name space1
ip link set space0 up
ip link set space1 netns $NAME up
ip addr add 172.18.1.1/24 dev space0
ip netns exec $NAME ip addr add 172.18.1.10/24 dev space1
ip netns exec $NAME ip route add default via 172.18.1.1 dev space1
ip netns exec $NAME ip route add 10.103.0.0/16 via 172.18.1.1 dev space1

mkdir -p /etc/netns/$NAME
echo 'nameserver 10.103.10.3' > /etc/netns/$NAME/resolv.conf

ip netns exec $NAME iptables -F
ip netns exec $NAME iptables -A INPUT -m conntrack --ctstate established,related -j ACCEPT
ip netns exec $NAME iptables -A INPUT -j REJECT
ip netns exec $NAME iptables -A FORWARD -m conntrack --ctstate established,related -j ACCEPT
ip netns exec $NAME iptables -A FORWARD -j REJECT

# ip netns exec protonvpn /bin/bash
ip netns exec $NAME /etc/init.d/openvpn start proton
