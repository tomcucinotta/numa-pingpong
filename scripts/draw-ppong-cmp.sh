#!/bin/bash

yrange="[0:]"

if [ "$1" == "-r" ]; then
    yrange="$2"
    shift
    shift
fi

fnames="$@"
if [ "$fnames" = "" ]; then
    fnames=ppong-pipe.dat
fi

prefix=ppong-$(echo "$fnames" | sed -e 's/ppong-//g; s/\.dat//g; s/ /-/g;')

for from in 0; do
    script=draw-cmp-$prefix-from$from.gp
    cat > $script <<EOF
#!/usr/bin/gnuplot

set terminal pdf color enhanced
set output '$prefix-from$from.pdf'

set xtics 4
set grid
set key above left

set ylabel 'Ping-pong time ({/Symbol m} s)'
set xlabel 'Second ping-pong core'
set yrange $yrange

# extend whiskers to abs min and max
set style boxplot fraction 1.0 labels off

set title 'Boxplot of ping-pong time (whiskers extend to min/max)'

# using x:data:width:level
plot [0:] 1/0 t '' \\
EOF
    for fname in $fnames; do
        name=${fname##ppong-}
        name=${name%%.dat}
        cat >> $script <<EOF
, "< grep '^$from ' $fname" u (0.0):3:(0.2):2 t '${name}: from core $from to others' w boxplot \\
EOF
    done
    echo "" >> $script
    gnuplot $script
done
