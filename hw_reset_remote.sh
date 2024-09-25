#!/bin/bash

set -o xtrace

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
# SERVID=(1 2 3 5 6 7 8 9 10)  # Example server IDs
SERVID="$@"  # Example server IDs
echo "Server IDs: ${SERVID}"

PROGRAM_FPGA=1
DRV_INSERT=1

for servid in ${SERVID[@]}; do 
  hostlist+="alveo-u55c-$(printf "%02d" $servid) "
done

##
## Program FPGA
##
echo "*** Activating server ..."
echo " ** "
parallel-ssh -H "$hostlist" "echo Login success!"

echo "*** Program FPGA: ${PROGRAM_FPGA}, Load Driver ${DRV_INSERT}"
echo " ** "
# disable timeout by -t 0, need ~ 5min
parallel-ssh -t 0 -H "$hostlist" -x '-tt' "cd ${SCRIPT_DIR} && ./hw_reset_local.sh ${PROGRAM_FPGA} ${DRV_INSERT}"
# parallel-ssh -t 0 -H "$hostlist" -x '-tt' -i "cd ${SCRIPT_DIR} && ./hw_reset_local.sh ${PROGRAM_FPGA} ${DRV_INSERT}"

echo "*** Done"
echo " ** "