LOGFILE1=/var/log/messages
LOGFILE2=/var/log/messages.1
if [ ! -f "${LOGFILE2}" ]; then
  LOGFILE2=
fi

WHEN='now - 1 day'
DATE=`date -d "$WHEN" +'%FT%T.000000%:z'`

NLINES=`tac $LOGFILE1 $LOGFILE2 | awk -v DATE="$DATE" '$1 < DATE { print NR; exit }'`
if [ -z "${NLINES}"]; then
  NLINES=`cat $LOGFILE1 $LOGFILE2 | wc -l`
fi

cat $LOGFILE2 $LOGFILE1 | tail -n $NLINES
