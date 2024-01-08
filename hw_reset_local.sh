#!/bin/bash
# set -o xtrace

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
QSFP_PORT=0
NUM_VFPGA_REGION=1
CLI_PATH=/opt/sgrt/cli
device_index=1

PROGRAM_FPGA=${1:-1}
DRV_INSERT=${2:-1}

if [ $PROGRAM_FPGA -eq 1 ]; then
  echo "*** Loading bitstream..."
  echo " ** "
  # ${CLI_PATH}/sgutil program vivado --bitstream /mnt/scratch/jiayli/distributed/build_2023_1229_2145_6FSM_noBF/bitstreams/cyt_top.bit
  # ${CLI_PATH}/sgutil program vivado --bitstream /mnt/scratch/jiayli/distributed/build_2023_1217_0913_6FSM_BF/bitstreams/cyt_top.bit
  # ${CLI_PATH}/sgutil program vivado --bitstream /mnt/scratch/jiayli/distributed/build_2023_1230_0852_8FSM_noBF/bitstreams/cyt_top.bit

  # ${CLI_PATH}/sgutil program vivado --bitstream /mnt/scratch/jiayli/distributed/build_2024_0103_1630_6FSM_BF_10RoutingChannel/bitstreams/cyt_top.bit
  ${CLI_PATH}/sgutil program vivado --bitstream /mnt/scratch/jiayli/distributed/build_2024_0103_2241_6FSM_BF_10RoutingChannel_Chord/bitstreams/cyt_top.bit
fi

if [ $DRV_INSERT -eq 1 ]; then
  echo "*** Loading driver..."
  echo " ** "
  qsfp_ip="DEVICE_1_IP_ADDRESS_HEX_${QSFP_PORT}"
  qsfp_mac="DEVICE_1_MAC_ADDRESS_${QSFP_PORT}"
  #get IP address
  # IP_address_cpu1=$($CLI_PATH/get/ifconfig | awk '$1 == "0:" {print $2}')
  # IP_address_cpu1_hex=$($CLI_PATH/common/address_to_hex IP $IP_address_cpu1)
  IP_address_0=$($CLI_PATH/get/network -d $device_index | awk '$1 == "1:" {print $2}')
  IP_address_0_hex=$($CLI_PATH/common/address_to_hex IP $IP_address_0)
  #get MAC address
  MAC_address_0=$($CLI_PATH/get/network -d $device_index | awk '$1 == "1:" {print $3}' | tr -d '()')
  MAC_address_0_hex=$($CLI_PATH/common/address_to_hex MAC $MAC_address_0)
  echo IP_address_0_hex=${IP_address_0_hex}
  echo MAC_address_0_hex=${MAC_address_0_hex}
  ${CLI_PATH}/sgutil program driver -m ${SCRIPT_DIR}/driver/coyote_drv.ko -p "ip_addr_q${QSFP_PORT}=${IP_address_0_hex},mac_addr_q${QSFP_PORT}=${MAC_address_0_hex}"
  # ${CLI_PATH}/sgutil program driver -m ${SCRIPT_DIR}/driver/coyote_drv.ko

  echo "*** Chmod FPGA ..."	
  echo " ** "
  # Either one should work
  # sudo ${CLI_PATH}/program/fpga_chmod 0
  ${CLI_PATH}/sgutil program coyote --regions ${NUM_VFPGA_REGION}
fi

echo "*** Done"
  echo " ** "
