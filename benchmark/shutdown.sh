#!/bin/bash

if [ $# -gt 2 ]; then
  echo "Usage: $0 <bench-only>"
  echo "If bench-only option is specified, only bench server will be shutted down."

  exit 1
fi

if [ -z "$1" ]; then
  BENCHONLY="n"
else
  BENCHONLY=$1
fi

echo "Stopping bench server..."
while IFS='' read -r line || [[ -n "$line" ]] ; do
  kill $line
done < "bench_pid"
rm bench_pid

if [ "$BENCHONLY" == "n" ]; then
  echo "Stopping local server..."
  ./scripts/stop-anna-local.sh y
fi