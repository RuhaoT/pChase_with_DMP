#!/bin/sh

#
# Initialisation
#

# Make sure to enter sudo mode before running this script!

# Generate a timestamp
timestamp=$(date +%Y%m%d-%H%M)

# Create a temporary directory
mkdir chase-$timestamp
cd chase-$timestamp
mkdir perf_data
mkdir framegraph

# Save some system information
uname -a > kernel.txt
cat /proc/cpuinfo > cpuinfo.txt
cat /proc/meminfo > meminfo.txt

# Configurable variables
chase=../chase
flamegraph=../../../../FlameGraph
count=0

#
# Benchmark
#

echo Benchmark initiated at $(date +%Y%m%d-%H%M) | tee -a chase.log

# command 'tee' is for output to both screen and file, '-a' is for append
for chain_size in 16m 32m 64m 128m 256m 512m 1g
do
    for thread in 1
    do
		for access in 'foward 1'
		do
            for prefetch in none
            do
                executecommand="$chase -c $chain_size -t $thread -a $access -f $prefetch -s 1.0 -e 5 -o csv"
                count=$((count+1))
                perf record -a -g -o perf_data/record_$count.data $executecommand 
                perf script -i ./perf_data/record_${count}.data | $flamegraph/stackcollapse-perf.pl > out.perf-folded
                flamegraphtitle=FlameGraph_${count}_${chain_size}_${thread}_${access}_${prefetch} 
                subtitle=Count:${count}_ChainSize:${chain_size}_Thread:${thread}_Access:${access}_Prefetch:${prefetch}
                $flamegraph/flamegraph.pl out.perf-folded > ./framegraph/$flamegraphtitle.svg --title "$flamegraphtitle" --subtitle "$subtitle"
                rm out.perf-folded
            done
        done
    done
done

echo Benchmark ended at $(date +%Y%m%d-%H%M) | tee -a chase.log

