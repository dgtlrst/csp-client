# CAN

## set bitrate

`sudo ip link set down can_alpha` (to check status, run `ip link show can_alpha`)

`sudo ip link set can_alpha type can bitrate 1000000`

`sudo ip link set up can_alpha` (to check status, run `ip link show can_alpha`)

`sudo ip link set can_alpha txqueuelen 1000`

## get channel names

sudo ethtool -e can_alpha raw off

cat /sys/class/net/can_alpha/peak_usb/can_channel_id

## set channel id to a name (must be different from 0 or 0xFF)

sudo ethtool -E can_alpha value {value} [e.g 2]

## set udev rules for program execution upon can initialisation

/etc/udev/rules.d/70-persistent-net.rules (file)

```
SUBSYSTEM=="net", ACTION=="add", DRIVERS=="peak_usb", KERNEL=="can*", PROGRAM="/bin/peak_usb_device_namer %k", NAME="%c"
```

where `peak_usb_device_namer` is the name of the script

## `peak_usb_device_namer` script

> **_NOTE:_**  The script must be executable and located in `/bin/` or `/usr/bin/`

```bash
#!/bin/sh
#
# External Udev program to rename peak_usb CAN interfaces according to the flashed device numbers.
#
# (C) 2023 PEAK-System GmbH by Stephane Grosjean 
#
# echo "\$1:\"$1\""
[ -z "$1" ] && exit 1
CAN_ID="/sys/class/net/$1/peak_usb/can_channel_id"
# echo "CAN_ID:\"$CAN_ID\""
if [ -f $CAN_ID ]; then
        devid=`cat $CAN_ID`
    # echo "devid:\"$devid\""
        # PCAN-USB specific: use "000000FF" instead of "FFFFFFFF" below
        if [ "$devid" != "00000000" -a "$devid" != "FFFFFFFF" ]; then
        if [ "$devid" == "00000022" ] ; then
            # can_name="can_alpha"
            can_name="can_beta"
            #  printf "cana\n" 0x${devid}
        elif [ "$devid" == "00000002" ] ; then
            # printf "canb\n" 0x${devid}
            # IF VALUE IS == 2 THEN CAN_NAME IS CAN_ALPHA
            can_name="can_alpha"
        elif [ "$devid" == "00000044" ] ; then
            # printf "canb\n" 0x${devid}
            can_name="can_alpha"
        else
            can_name="$(printf "can%d\n" 0x${devid})"
            # printf "can%d\n" 0x${devid}
        fi
        echo "${can_name}"
                exit 0
        fi
    exit 1
fi
exit 1
```

## to initially trigger execution of the script

`sudo udevadm control --reload-rules`

and then trigger udev to apply the rules:

`sudo udevadm trigger`

or restart the service:

`sudo systemctl restart systemd-udevd.service`

*comment: the last command here is called only if the user does not want to reboot, but after that it works automatically *
