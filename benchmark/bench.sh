#!/bin/bash

if [ $# -lt 4 ]; then
  echo "Usage: $0 load/run workload_letter object_size concurrency"
  echo "If no build option is specified, the test will default to not building."

  exit 1
fi

WORKLOAD_CLS=$2
WORKLOAD=workload$WORKLOAD_CLS
OBJECT_SIZE=$3
CONCURRENCY=$4
DONE_PARAMS=""
if [ "$1" == "load" ]; then
  CONF=./benchmark/warm_${OBJECT_SIZE}.conf
else
  CONF=./benchmark/${WORKLOAD}_${OBJECT_SIZE}.conf
  OUTPUT=./data/anna${WORKLOAD_CLS}_${OBJECT_SIZE}_c${CONCURRENCY}_summary.txt
  rm -f $OUTPUT
  DONE_PARAMS=":$OUTPUT"
fi

CMD=`cat $CONF`

mkdir data

echo "Starting benchmark driver..."
./benchmark/shutdown.sh y
./build/target/benchmark/anna-bench &
BPID=$!
echo $BPID > bench_pid

echo "Trigger benchmark using command: $CMD ..."
/usr/bin/expect <<EOF
spawn ./build/target/benchmark/anna-bench-trigger $CONCURRENCY
expect "command>"
send "$CMD\n"
expect "command>"
send "STATS:WAITDONE:$CONCURRENCY$DONE_PARAMS\n"
expect eof
EOF
echo ""
