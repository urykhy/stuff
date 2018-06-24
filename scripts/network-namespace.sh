#!/bin/bash

NAME=private

ip netns add  $NAME
ip netns exec $NAME ip addr add 127.0.0.1/8 dev lo
ip netns exec $NAME ip link set lo up

# space - это интерфейс для выхода из NS в мир
ip link add space0 type veth peer name space1
ip link set space0 up
ip link set space1 netns $NAME up
ip addr add 172.17.1.1/24 dev space0
ip netns exec $NAME ip addr add 172.17.1.10/24 dev space1
ip netns exec $NAME ip route add default via 172.17.1.1 dev space1
ip netns exec $NAME ip route add 10.103.0.0/16 via 172.17.1.1 dev space1

mkdir -p /etc/netns/$NAME
echo 'nameserver 10.103.10.3' > /etc/netns/$NAME/resolv.conf

ip netns exec $NAME iptables -A INPUT -m conntrack --ctstate established,related -j ACCEPT
ip netns exec $NAME iptables -A INPUT -p tcp -m multiport --dports 6771,6881 -j ACCEPT
ip netns exec $NAME iptables -A INPUT -p udp -m multiport --dports 6771,6881 -j ACCEPT
ip netns exec $NAME iptables -A INPUT -j REJECT

# ip netns exec private /bin/bash
# после запуска vpn - дефолтный маршрут будет вести в VPN
ip netns exec $NAME /etc/init.d/openvpn start private
