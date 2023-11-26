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
uname -a >kernel.txt
cat /proc/cpuinfo >cpuinfo.txt
cat /proc/meminfo >meminfo.txt

# Configurable variables
output=chase.csv
chase=../chase
flamegraph=../../../../FlameGraph
count=0
pathback=../../

#
# Benchmark
#

echo Benchmark initiated at $(date +%Y%m%d-%H%M) | tee -a chase.log

# command 'tee' is for output to both screen and file, '-a' is for append
# $chase -o hdr | tee -a $output
for chain_size in 128m 128m 128m; do
    for thread in 1; do
        for access in 'forward 1'; do
            for prefetch in none; do
                mkdir perf_data/execution_$count
                cd perf_data/execution_$count
                $pathback$chase -c $chain_size -t $thread -a $access -f $prefetch -i 1 -e 5 -o csv | tee -a $pathback$output

                # sleep a little to wait perf to dump data
                sleep 1

                # generate symbol map
                for file in $(ls *.data); do
                    echo "Generating symbol map for $file"
                    perf script -i $file >${file%.*}.script
                    # perf script --header -i ${file%.*}.script > ${file%.*}.map
                done

                # draw flamegraph
                # only select file with .data suffix
                for file in $(ls *.data); do
                    echo "Printing flamegraph for $file"
                    perf script -i ./$file | $pathback$flamegraph/stackcollapse-perf.pl >out.perf-folded
                    flamegraphtitle=FlameGraph_${count}_${chain_size}_${thread}_${access}_${prefetch}
                    subtitle=Count:${count}_ChainSize:${chain_size}_Thread:${thread}_Access:${access}_Prefetch:${prefetch}
                    $pathback$flamegraph/flamegraph.pl out.perf-folded --title "$flamegraphtitle" --subtitle "$subtitle" >$pathback/framegraph/${file%.*}_${flamegraphtitle}.svg
                    rm out.perf-folded
                done

                cd $pathback
                count=$((count + 1))
            done
        done
    done
done

echo Benchmark ended at $(date +%Y%m%d-%H%M) | tee -a chase.log
