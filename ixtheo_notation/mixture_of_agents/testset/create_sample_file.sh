entries=150
sample_file_name="sampled_notations.txt"
> ${sample_file_name}
for notation_file in $(ls notation_ppns/*.txt); do
    lines_in_file=$(wc -l < ${notation_file})
    sampling_rate=$(echo "scale=4; ${entries}/${lines_in_file}" | bc)
    echo $(basename ${notation_file}) ": $sampling_rate"
    cat ${notation_file}  | awk 'BEGIN {srand()} !/^$/ { if (rand() <= '${sampling_rate}') print $0}' >> ${sample_file_name}
done
