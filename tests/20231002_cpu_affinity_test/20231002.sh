#!bin/sh


# Configurable variables
output=chase.csv


# Create a temporary directory
timestamp=$(date +%Y%m%d-%H%M)
mkdir chase-$timestamp
cd chase-$timestamp

# Save some system information
uname -a > kernel.txt
cat /proc/cpuinfo > cpuinfo.txt
cat /proc/meminfo > meminfo.txt

echo Experiment initiated at $(date +%Y%m%d-%H%M) | tee -a chase.log
for chain_size in 8k 
do
    for loop_size in 0
    do
		for access in random 
		do
            for prefetch in none 
            do
                for thread in 1 2 4 8 16 32 64 128 256 
                # append thread number to the name of output file
                do
                    output=chase-$thread.csv
                    ../chase -c $chain_size -g $loop_size -a $access -f $prefetch -s 1.0 -e 5 -o csv -t $thread | tee -a $output
                done
            done
        done
    done
done


echo Experiment ended at $(date +%Y%m%d-%H%M) | tee -a chase.log