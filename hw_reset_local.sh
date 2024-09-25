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
  ${CLI_PATH}/sgutil program vivado --bitstream ${SCRIPT_DIR}/hw/build/bitstreams/cyt_top.bit
fi

if [ $DRV_INSERT -eq 1 ]; then
  echo "*** Loading driver..."
  echo " ** "
  ${CLI_PATH}/sgutil program driver -i ${SCRIPT_DIR}/driver/coyote_drv.ko -p "ip_addr_q${QSFP_PORT}=${DEVICE_1_IP_ADDRESS_HEX_0},mac_addr_q${QSFP_PORT}=${DEVICE_1_MAC_ADDRESS_0}"

  # We need this to add read and write permissions for everybody on the fpga device file
  echo "*** Chmod FPGA ..."	
  echo " ** "
  sudo ${CLI_PATH}/program/fpga_chmod 0
fi

echo "*** Done"
echo " ** "
