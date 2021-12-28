#!/bin/bash

sudo ifconfig wlan0 up 192.168.1.1 netmask 255.255.255.0
sudo ln -s /var/run/dhcp-server/dhcpd.pid /var/run/dhcpd.pid
sudo dhcpd wlan0

#exit
