#!/bin/bash

if [ $# -gt 2 ]; then
  echo "Usage: $0 <local>"
  echo "If local option is specified, only bench server will be shutted down."

  exit 1
fi

if [ -z "$1" ]; then
  LOCAL="n"
else
  LOCAL=$1
fi

echo "Stopping bench server..."
while IFS='' read -r line || [[ -n "$line" ]] ; do
  kill $line
done < "bench_pid"
rm bench_pid

if [ "$LOCAL" == "y" ]; then
  echo "Stopping local server..."
  ./scripts/stop-anna-local.sh y
fi