# libcsp-2-linux
A linux libcsp 2.0 client/server, utilizing can / uart communication interfaces.

## system information

Tested on:

**OS**: Ubuntu 24.04.2 LTS x86_64
**Host**: HP EliteBook 850 G6
**Kernel**: 6.11.0-21-generic
**Uptime**: 4 days, 20 hours, 44 mins
**Packages**: 1752 (dpkg), 10 (snap)
**Shell**: bash 5.2.21
**Resolution**: 1920x1080
**Terminal**: /dev/pts/3
**CPU**: Intel i7-8565U (8) @ 4.600GHz
**GPU**: Intel WhiskeyLake-U GT2 [UHD Graphics 620]
**GPU**: AMD ATI Radeon 540X/550X/630 / RX 640 / E9171 MCM
**Memory**: 3593MiB / 15799MiB

## driver information

`sudo ip link set can_alpha type can bitrate 1000000`

### peak_usb

filename:       /lib/modules/6.11.0-21-generic/kernel/drivers/net/can/usb/peak_usb/peak_usb.ko.zst
license:        GPL v2
description:    CAN driver for PEAK-System USB adapters
author:         Stephane Grosjean <s.grosjean@peak-system.com>
srcversion:     B978D19D894F279E2B92CAA
depends:        can-dev
retpoline:      Y
intree:         Y
name:           peak_usb
vermagic:       6.11.0-21-generic SMP preempt mod_unload modversions

### can-dev

filename:       /lib/modules/6.11.0-21-generic/kernel/drivers/net/can/dev/can-dev.ko.zst
author:         Wolfgang Grandegger <wg@grandegger.com>
license:        GPL v2
description:    CAN device driver interface
alias:          rtnl-link-can
srcversion:     F7BA7944625E669E77825E0
depends:
retpoline:      Y
intree:         Y
name:           can_dev
vermagic:       6.11.0-21-generic SMP preempt mod_unload modversions

### pcan

filename:       /lib/modules/6.11.0-21-generic/misc/pcan.ko
license:        GPL
version:        Release_20250213_n
description:    Netdev driver for PEAK-System CAN interfaces
author:         klaus.hitschler@gmx.de
author:         s.grosjean@peak-system.com
srcversion:     E384788D80BD3973893A78B
depends:        i2c-algo-bit,can-dev
retpoline:      Y
name:           pcan
vermagic:       6.11.0-21-generic SMP preempt mod_unload modversions
