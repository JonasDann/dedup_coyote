#!/bin/bash
# set -o xtrace

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# SERVID=(1 2 3 5 6 7 8 9 10)  # Example server IDs
SERVID="$@"
echo "Server IDs: ${SERVID}"
for servid in ${SERVID[@]}; do 
  hostlist+="alveo-u55c-$(printf "%02d" $servid) "
done

# Define the output file
OUTPUT_FILE="${SCRIPT_DIR}/server_ip.csv"
# Function to format the output
format_output() {
  local server=$1
  local output=$2

  echo "$server,$output"
}

# Clear the output file
> $OUTPUT_FILE
# Run the script on each server and collect the output
for hostname in ${hostlist[@]}; do
  output=$(ssh $hostname -tt "cd ${SCRIPT_DIR} && ./get_ip_local.sh")
  formatted_output=$(format_output "$hostname" "$output")
  echo "$formatted_output" >> $OUTPUT_FILE
done

# Display the results
echo "Results saved in $OUTPUT_FILE"
cat $OUTPUT_FILE
