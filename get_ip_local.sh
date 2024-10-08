#!/bin/bash
# set -o xtrace

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
CLI_PATH=/opt/sgrt/cli
device_index=1

IP_address_cpu1=$($CLI_PATH/get/ifconfig | awk '$1 == "1:" {print $2}')
IP_address_cpu1_hex=$($CLI_PATH/common/address_to_hex IP $IP_address_cpu1)

IP_address_0=$($CLI_PATH/get/network -d $device_index | awk '$1 == "1:" {print $2}')
IP_address_0_hex=$($CLI_PATH/common/address_to_hex IP $IP_address_0)
echo ${IP_address_cpu1},${IP_address_0}