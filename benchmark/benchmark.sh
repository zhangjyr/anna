#!/bin/bash

if [ $# -gt 5 ]; then
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

cp conf/anna-benchmark.yml conf/anna-config.yml

echo "Starting benchmark driver..."
/home/ubuntu/hydro-project/anna
./build/target/benchmark/anna-bench 1>log 2>&1 &
echo $BPID >> pids

echo "Trigger benchmark command..."
/usr/bin/expect <<EOD
spawn ./build/target/benchmark/anna-bench-trigger $CONCURRENCY
expect "command> "
send "$CMD\n"
EOD
echo ""
