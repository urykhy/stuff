#!/bin/bash

NAME=protonvpn

__already_active=`sudo ip netns exec $NAME ip a show proton | grep proton | grep ',UP,' | wc -l`
if [ "$__already_active" -eq "0" ]; then
    zenity --notification --text "start proton vpn ..."
    sudo ~/bin/network-namespace.sh
    for i in {1..10}
    do
        sleep 1;
        __is_up=`sudo ip netns exec $NAME ip a show proton | grep proton | grep ',UP,' | wc -l`
        if [ "$__is_up" -eq "1" ]; then
            break
        fi
    done
    if [ "$__is_up" -eq "0" ]; then
        zenity --notification --text "vpn problem"
        exit
    fi
fi
sudo ip netns exec $NAME su ury -c "MOZ_WEBRENDER=1 firefox --no-remote --profile ~/.mozilla/firefox/i4dng3ed.yandex"
