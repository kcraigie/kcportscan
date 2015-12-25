#!/bin/bash

NUM_LISTENERS=99
LISTEN_HOST=127.0.0.1
SCAN_BLOCK_PREFIX=28
SCAN_TIMEOUT=200

for i in `eval echo "{0..$NUM_LISTENERS}"`; do
    ports[$i]="$((((RANDOM + RANDOM) % 63001) + 2000))"
done

echo -n "Spawning `expr $NUM_LISTENERS + 1` random listeners on $LISTEN_HOST"
for port in ${ports[@]}; do
    ./kclisten $LISTEN_HOST:$port 1>/dev/null &
    echo -n "."
done
echo

echo "Spawning kcportscan on $LISTEN_HOST/$SCAN_BLOCK_PREFIX..."
./kcportscan -4 $LISTEN_HOST/$SCAN_BLOCK_PREFIX -t $SCAN_TIMEOUT >&/tmp/kcportscan.log &
job_id=$!

echo -n "Waiting for kcportscan ($job_id) to finish..."
wait $job_id
echo

echo -n "See whether kcportscan found all of the random ports"
for port in ${ports[@]}; do
    grep -q "$LISTEN_HOST:$port accepted " /tmp/kcportscan.log
    if [[ $? != 0 ]]; then
        echo "FAILURE: kcportscan missed finding $LISTEN_HOST:$port!"
        exit 1
    fi
    echo -n "."
done
echo "SUCCESS!"

exit 0

# killall kclisten
