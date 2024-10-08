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

  if [ -d /tools/Xilinx/Vivado/2022.1 ]; then
    source /tools/Xilinx/Vivado/2022.1/settings64.sh
  else
    source /tools/Xilinx/Vivado/2024.1/settings64.sh
  fi
  
  ${CLI_PATH}/sgutil program vivado --bitstream ${SCRIPT_DIR}/hw/build/bitstreams/cyt_top.bit
fi

if [ $DRV_INSERT -eq 1 ]; then
  IP_address_0=$($CLI_PATH/sgutil get network -d $device_index | awk '$1 == "1:" {print $2}')
  MAC_address_0=$($CLI_PATH/sgutil get network -d $device_index | awk '$1 == "1:" {print $3}' | tr -d '()')
  DEVICE_1_IP_ADDRESS_HEX_0=$($CLI_PATH/common/address_to_hex IP $IP_address_0)
  DEVICE_1_MAC_ADDRESS_0=$($CLI_PATH/common/address_to_hex MAC $MAC_address_0)

  echo "*** Loading driver..."
  echo " ** "
  sudo rmmod coyote_drv
  sudo insmod ${SCRIPT_DIR}/driver/coyote_drv.ko ip_addr_q${QSFP_PORT}=${DEVICE_1_IP_ADDRESS_HEX_0} mac_addr_q${QSFP_PORT}=${DEVICE_1_MAC_ADDRESS_0}

  # We need this to add read and write permissions for everybody on the fpga device file
  echo "*** Chmod FPGA ..."	
  echo " ** "
  sudo ${CLI_PATH}/program/fpga_chmod 0
fi

echo "*** Done"
echo " ** "
