#!bin/sh

# variables
perf_data_dir=perf_data_raw
event_list="cycles L1-dcache-loads L1-dcache-load-misses"
analysis_result=analysis_result.csv

# browse all folder name started with 'chase-'
for folder in chase-*; do
    # go to folder named $folder
    cd $folder

    # remove the old analysis result file
    rm -f $analysis_result
    # add a header to the analysis result file
    unset first
    for event in $event_list; do
        if [ -z "$first" ]; then
            # add the first event to the first line of the analysis result
            echo -n "$event" >> ./$analysis_result
            first="set"
        else
            # add the remaining events to subsequent lines with a leading comma
            echo -n ",$event" >> ./$analysis_result
        fi
    done
    # add a new line
    echo "" >> ./$analysis_result

    # go to the perf_data_dir
    cd $perf_data_dir

    # create each line of the analysis result
    for perf_data_file in perf_*.data; do
        # create a new line
        echo "" >> ../$analysis_result
    done

    # browse all perf data files
    for perf_data_file in perf_*.data; do
        # report status
        echo "Processing $perf_data_file"

        # get the number of the perf data file
        number=$(echo $perf_data_file | cut -d'_' -f2 | cut -d'.' -f1)
        # the output line should start with the number of the perf data file + 1
        number=$((number + 2))

        # find the symbol of the JIT function

        # find the sentence containing the symbol of the JIT function
        # only use the first sentence because grep produce too many lines
        temp=$(perf script -i $perf_data_file | grep tmp | head -n 1)

        # extract the first string, which is the symbol of the JIT function
        function_symbol=$(echo $temp | cut -d' ' -f1)

        # extract the number of each event
        unset first
        for event in $event_list; do
            temp=$(perf report -i ${perf_data_file} -S ${function_symbol} --stats | grep -A 1 "${event}" |tail -n 1| awk '{print $NF}')
            # add the number to the number line of the analysis result
            # if it is the first number, don't add a leading comma
            if [ -z "$first" ]; then
                sed -i "${number}s/$/${temp}/" ../$analysis_result
                first="set"
            else
                sed -i "${number}s/$/,${temp}/" ../$analysis_result
            fi
        done
    done

    # go out of the perf_data_dir
    cd ..

    # go out of the folder named $folder
    cd ..
done