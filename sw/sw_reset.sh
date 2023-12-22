#!/bin/bash
set -o xtrace
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
BUILD_DIR="${SCRIPT_DIR}/build"
rm -rf ${BUILD_DIR}
mkdir ${BUILD_DIR} && cd ${BUILD_DIR}
# /usr/bin/cmake ${SCRIPT_DIR} -DTARGET_DIR=${SCRIPT_DIR}/examples/perf_rdma
# /usr/bin/cmake ${SCRIPT_DIR} -DTARGET_DIR=${SCRIPT_DIR}/examples/dedup_bench2
/usr/bin/cmake ${SCRIPT_DIR} -DTARGET_DIR=${SCRIPT_DIR}/examples/dedupnew_bench1
make