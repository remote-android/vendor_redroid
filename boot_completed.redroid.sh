#!/system/bin/sh
ndc resolver setnetdns eth0 "`getprop ro.kernel.redroid.dns.domain`" `getprop ro.kernel.redroid.dns` 8.8.8.8 8.8.4.4 
