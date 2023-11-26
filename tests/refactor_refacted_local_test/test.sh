#!/bin/sh

#
# Initialisation
#

# Configurable variables
output=chase.csv
chase=../chase

# Generate a timestamp
timestamp=$(date +%Y%m%d-%H%M)

# Create a temporary directory
mkdir chase-$timestamp
cd chase-$timestamp

# Save some system information
uname -a > kernel.txt
cat /proc/cpuinfo > cpuinfo.txt
cat /proc/meminfo > meminfo.txt


#
# Benchmark
#

echo Benchmark initiated at $(date +%Y%m%d-%H%M) | tee -a chase.log

# command 'tee' is for output to both screen and file, '-a' is for append
$chase -o hdr | tee $output
for chain_size in 8k 16k 64k 256k 512k
do
    for loop_size in 0
    do
		for access in random "forward 1"
		do
            for prefetch in none
            do
                for iterations in 10
                do
                    $chase -c $chain_size -g $loop_size -a $access -f $prefetch -i $iterations -s 1.0 -e 1 -o csv | tee -a $output
                done
            done
        done
    done
done

echo Benchmark ended at $(date +%Y%m%d-%H%M) | tee -a chase.log

