#!/bin/bash
clear
rm zpsm_kern.log
sudo grep "ZPSM LOG::" /var/log/messages > zpsm_kern.log
#sudo rm /var/log/messages
