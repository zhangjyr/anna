#!/bin/bash

if [ $# -lt 4 ]; then
  echo "Usage: $0 load/run workload_letter object_size concurrency"
  echo "If no build option is specified, the test will default to not building."

  exit 1
fi

WORKLOAD=workload$2
OBJECT_SIZE=$3
CONCURRENCY=$4
if [ "$1" == "load" ]; then
  CONF=./benchmark/warm_${OBJECT_SIZE}.conf
else
  CONF=./benchmark/${WORKLOAD}_${OBJECT_SIZE}.conf
fi

CMD=`cat $CONF`

# cp conf/anna-benchmark.yml conf/anna-config.yml

echo "Starting benchmark driver..."
./build/target/benchmark/anna-bench &
BPID=$!
echo $BPID > bench_pid

echo "Trigger benchmark using command: $CMD ..."
/usr/bin/expect <<EOF
spawn ./build/target/benchmark/anna-bench-trigger $CONCURRENCY
expect "command>"
send "$CMD\n"
expect eof
EOF
echo ""
