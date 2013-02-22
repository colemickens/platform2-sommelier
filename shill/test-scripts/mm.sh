#!/bin/dash

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
IMODEMS="$IMODEM $IMODEM_CDMA $IMODEM_GSM_CARD $IMODEM_GSM_NETWORK $IMODEM_GOBI"

MTYPE_GSM=1
MTYPE_CDMA=2

iface2rootobj () {
	echo "/$1" | sed -e 's!\.!/!g'
}

dbus () {
	local dest=$1
	local obj=$2
	local meth=$3
	shift 3

	dbus-send --system --print-reply --fixed \
        --dest=$dest "$obj" "$meth" "$@"
}

modemmanagers () {
	dbus $IDBUS $DBUS $IDBUS.ListNames | awk '/ModemManager/ { print $2 }'
}

modems () {
	local mm=$1
	dbus $mm $(iface2rootobj $mm) $IMM.EnumerateDevices | awk '{print $2}'
}

modemprops () {
	local mm=$1
	local modem=$2
	for i in $IMODEMS; do
		dbus $mm $modem $IDBUS_PROPERTIES.GetAll string:$i 2>/dev/null \
			| awk '/[^[:space:]] [^[:space:]]/ {print $N}'
	done
}

modemprop () {
	local mm=$1
	local modem=$2
	local prop=$3
	modemprops $mm $modem | grep /$prop | awk '{print $2}'
}
