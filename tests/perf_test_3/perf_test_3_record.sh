#!/bin/sh

#
# Initialisation
#

# Generate a timestamp
timestamp=$(date +%Y%m%d-%H%M)

# Create a temporary directory
mkdir chase-$timestamp
cd chase-$timestamp

# Save some system information
uname -a >kernel.txt
cat /proc/cpuinfo >cpuinfo.txt
cat /proc/meminfo >meminfo.txt

# Configurable variables
chase=../chase
output=chase_result.csv
execution_count=0
backpath=../
perf_data_dir=perf_data_raw
event_list="{cycles,cache-references,cache-misses}"
perf_sample_rate=400

mkdir $perf_data_dir

#
# Benchmark
#

echo Benchmark initiated at $(date +%Y%m%d-%H%M) | tee -a chase.log

# command 'tee' is for output to both screen and file, '-a' is for append
$chase -o hdr | tee $output
# rm *.data
cd $perf_data_dir
for chain_size in 256m 512m 1g; do
    for thread in 1; do
        for loop_size in 0 25 50; do
            for access in "forward 1" "forward 10" "forward 100" "reverse 1" "reverse 10" "reverse 100" "random"; do
                for prefetch in none nta t0 t1 t2; do
                    for experiment in 1; do
                        for iterations in 10; do 
                            # Prepair benchmark execution command
                            execution_command="$backpath$chase -c $chain_size -t $thread -g $loop_size -a $access -f $prefetch -e $experiment -i $iterations -o csv"

                            # profile the execution
                            perf record -F $perf_sample_rate -e $event_list --call-graph dwarf -g -o perf_${execution_count}.data $execution_command | tee -a $backpath$output

                            # Increment execution count
                            execution_count=$((execution_count + 1))
                        done
                    done
                done
            done
        done
    done
done
cd ..

echo Benchmark ended at $(date +%Y%m%d-%H%M) | tee -a chase.log
