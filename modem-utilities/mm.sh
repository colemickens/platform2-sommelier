#!/bin/dash
# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Library for talking to ModemManager from sh. Forked (ugh) from
# flimflam/test, which has diverged and still exists.

MM=${MM:-/org/chromium/ModemManager}
IDBUS=org.freedesktop.DBus
DBUS=/org/freedesktop/DBus
IDBUS_PROPERTIES=$IDBUS.Properties
IMM=org.freedesktop.ModemManager
IMODEM=$IMM.Modem
IMODEM_SIMPLE=$IMODEM.Simple
IMODEM_CDMA=$IMODEM.Cdma
IMODEM_GSM=$IMODEM.Gsm
IMODEM_GSM_CARD=$IMODEM_GSM.Card
IMODEM_GSM_NETWORK=$IMODEM_GSM.Network
IMODEM_GOBI=org.chromium.ModemManager.Modem.Gobi
IMODEMS="$IMODEM $IMODEM_SIMPLE $IMODEM_CDMA $IMODEM_GSM $IMODEM_GOBI"
DEST=`echo $MM | sed -e 's!^/!!g' -e 's!/!.!g'`

MTYPE_GSM=1
MTYPE_CDMA=2

dbus () {
	local obj=$1
	local method=$2
	shift 2

	dbus-send --system --print-reply --fixed \
        --dest=$DEST "$obj" "$method" $*
}

modems () {
	dbus $MM $IMM.EnumerateDevices | awk '{print $2}'
}

modemmanagers () {
	dbus-send --system --print-reply --fixed \
	--dest=$IDBUS $DBUS $IDBUS.ListNames \
	| awk '/ModemManager/ { print $2 }'
}

modemprops () {
	local modem=$1
	for i in $IMODEMS; do
		dbus $modem $IDBUS_PROPERTIES.GetAll string:$i 2>/dev/null \
			| awk '/[^[:space:]] [^[:space:]]/ {print $N}'
	done
}

modemprop () {
	local modem=$1
	local prop=$2
	modemprops $modem | grep /$prop | awk '{print $2}'
}

modem_status () {
        local modem=$1
        local prop=$2
        if [ -n $modem ] ; then
                dbus $modem $IMODEM_SIMPLE.GetStatus |\
                      awk "/\/$prop/ {print \$2}"
        fi
}

modem_activation_state () {

        if [ -n $modem ] ; then
                modem_status $modem activation_state
        fi
}
