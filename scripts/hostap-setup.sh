#!/bin/bash

sudo service network-manager stop
sudo cp dhcpd.conf /etc/dhcp/.
sudo hostapd hostapd.conf
#exit
