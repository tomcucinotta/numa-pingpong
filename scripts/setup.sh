#!/bin/bash

# author: Tommaso Cucinotta <tommaso.cucinotta@santannapisa.it>

if [ -f /sys/kernel/debug/sched_features ]; then
    echo HRTICK | sudo tee /sys/kernel/debug/sched_features
elif [ -f /sys/kernel/debug/sched/features ]; then
    echo HRTICK | sudo tee /sys/kernel/debug/sched/features
fi

if [ -d /sys/devices/system/cpu/intel_pstate ]; then
    if [ -e /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
	echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo
    fi

    echo 100 > /sys/devices/system/cpu/intel_pstate/max_perf_pct
    echo 100 > /sys/devices/system/cpu/intel_pstate/min_perf_pct

    for f in no_turbo turbo_pct min_perf_pct max_perf_pct; do
	echo $f=$(cat /sys/devices/system/cpu/intel_pstate/$f)
    done
fi

if [ -d /sys/devices/system/cpu/cpu0/cpufreq ]; then
    for c in $(ls -d /sys/devices/system/cpu/cpu[0-9]*); do
	echo performance > $c/cpufreq/scaling_governor
	if [ -e $c/cpufreq/energy_performance_preference ]; then
	    echo performance > $c/cpufreq/energy_performance_preference
	fi
	if [ -e $c/power/energy_perf_bias ]; then
	    echo performance > $c/power/energy_perf_bias
	fi
	if [ -f $c/power/pm_qos_resume_latency_us ]; then
	    echo "n/a" > $c/power/pm_qos_resume_latency_us
	fi
    done

    echo "#CPU scaling_governor scaling_cur_freq energy_perf_pref energy_perf_bias resume_latency"
    for c in $(ls -d /sys/devices/system/cpu/cpu[0-9]*); do
	echo $c $(cat $c/cpufreq/scaling_governor) $(cat $c/cpufreq/scaling_cur_freq) $(cat $c/cpufreq/energy_performance_preference 2> /dev/null) $(cat $c/power/energy_perf_bias 2> /dev/null) $(cat $c/power/pm_qos_resume_latency_us 2> /dev/null)
    done
fi
