#!/bin/bash

if [ $# -gt 5 ]; then
  echo "Usage: $0 load/run workload_letter object_size concurrency"
  echo "If no build option is specified, the test will default to not building."

  exit 1
fi

echo "Starting local server..."
./scripts/start-anna-local.sh n n # Don't build, don't start user

./benchmark/bench.sh $1 $2 $3 $4

