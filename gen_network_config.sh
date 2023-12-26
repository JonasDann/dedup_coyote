#!/bin/bash

# set -o xtrace

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
SERVID=(6 8 9 10)  # Example server IDs
# SERVID=(1 2 3 5 6 7 8 9 10)  # Example server IDs

GEN_ROUTING_TABLE=1
GEN_NETWORK_CONFIG=1

for servid in ${SERVID[@]}; do 
  hostlist+="alveo-u55c-$(printf "%02d" $servid) "
done

##
## Generate Routing Table
##
if [ $GEN_ROUTING_TABLE -eq 1 ]; then
  echo "*** Generating Routing Table..."
  echo " ** "
  # python3 routing_table_gen.py --hostlist ${hostlist[@]} --mode hop_test --hop-count 3
	python3 routing_table_gen.py --hostlist ${hostlist[@]} --mode equal_divide
fi

##
## Generate RDMA connection config
##
if [ $GEN_NETWORK_CONFIG -eq 1 ]; then
  echo "*** Generating All Config..."
  echo " ** "
  python3 rdma_connection_gen.py
fi

echo "*** Done"
echo " ** "