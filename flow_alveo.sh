#!/bin/bash

# set -o xtrace

# This script is basically:
# ./get_ip_remote.sh
# ./hw_reset_remote.sh
# ./gen_network_config.sh

##
## Args
##
if ! [ -x "$(command -v vivado)" ]; then
	echo "Vivado does NOT exist in the system."
	exit 1
fi
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

##
## Paths and Server IDs
##
IP_OUTPUT_FILE="${SCRIPT_DIR}/server_ip.csv"
SERVID=(6 8 10)  # Example server IDs

for servid in ${SERVID[@]}; do 
  hostlist+="alveo-u55c-$(printf "%02d" $servid) "
done

##
## Actions
##
GET_IP=1
PROGRAM_FPGA=1
DRV_INSERT=1
GEN_ROUTING_TABLE=1
GEN_NETWORK_CONFIG=1

##
## Test Connections
##
echo "*** Activating server ..."
echo " ** "
parallel-ssh -H "$hostlist" "echo Login success!"

##
## Get Server IPs
##
if [ $GET_IP -eq 1 ]; then
  echo "*** Getting all server IPs..."
  echo " ** "
  # Function to format the output
  format_output() {
    local server=$1
    local output=$2

    echo "$server,$output"
  }

  # Clear the output file
  > $IP_OUTPUT_FILE
  # Run the script on each server and collect the output
  for hostname in ${hostlist[@]}; do
    output=$(ssh $hostname -tt "cd ${SCRIPT_DIR} && ./get_ip_local.sh")
    formatted_output=$(format_output "$hostname" "$output")
    echo "$formatted_output" >> $IP_OUTPUT_FILE
  done
  # Display the results
  echo "Results saved in $IP_OUTPUT_FILE"
  cat $IP_OUTPUT_FILE
fi

##
## Program FPGA and Load driver
##
echo "*** Programming FPGA: ${PROGRAM_FPGA}, Loading Driver ${DRV_INSERT}"
echo " ** "
# disable timeout by -t 0, need ~ 5min
parallel-ssh -t 0 -H "$hostlist" -x '-tt' "cd ${SCRIPT_DIR} && ./hw_reset_local.sh ${PROGRAM_FPGA} ${DRV_INSERT}"
# parallel-ssh -t 0 -H "$hostlist" -x '-tt' -i "cd ${SCRIPT_DIR} && ./hw_reset_local.sh ${PROGRAM_FPGA} ${DRV_INSERT}"

##
## Generate network config
##
if [ $GEN_ROUTING_TABLE -eq 1 ]; then
  echo "*** Generating Routing Table..."
  echo " ** "
  # python3 routing_table_gen.py --hostlist ${hostlist[@]} --mode hop_test --hop-count 2
	python3 routing_table_gen.py --hostlist ${hostlist[@]} --mode equal_divide
fi

if [ $GEN_NETWORK_CONFIG -eq 1 ]; then
  echo "*** Generating All Config..."
  echo " ** "
  python3 rdma_connection_gen.py
fi

echo "*** Done"
echo " ** "
