#!/bin/bash

mydir=$(dirname $(realpath $0))

if [ -x ./numa-pingpong ]; then
    ppong_cmd=./numa-pingpong
elif type -t numa-pingpong; then
    ppong_cmd=numa-pingpong
elif [ -x ../src/numa-pingpong ]; then
    ppong_cmd=../src/numa-pingpong
else
    echo "Cannot find the numa-pingpong executable in .:../src:$PATH"
    exit 1
fi

echo "Found numa-pingpong command: $ppong_cmd"

suff="condvar"

if [ "$1" != "" ]; then
    suff="$1"
fi

sudo $mydir/setup.sh

N=$(ls /sys/devices/system/cpu/ | grep -c 'cpu[0-9]\+')
R=10000

echo -n > ppong-"$suff".dat

for ((i = 0; i < N; i++)); do
    echo "Ping-pong from core $i ..."
    for ((j = 0; j < N; j++)); do
	ii=$(printf %02d $i)
	jj=$(printf %02d $j)
	for ((k = 0; k < 10; k++)); do
	    echo "$i $j `sudo chrt -f 10 $ppong_cmd -s $suff -r $R -c $i,$j`" >> ppong-"$suff".dat
	done
    done
done

sudo $mydir/teardown.sh
