# DedupCore in Coyote

## Run Dedup with Coyote
### Step 1 Hardware
First step is to generate verilog files for DedupCore from Scala(SpinalHDL). (https://github.com/jia-yli/distributed-dedup)

There are some generated verilog files for DedupCore in different configurations. They are under `dedup_verilogs`:
1. Hash Table size, 32768 entry x 8(avg linked list len) or 8GiB memory space in total. The prototype uses 32768 entry for performance measurements.
2. Number of FSM: 6 or 8
3. With/without Bloom Filter(only for 6 FSM, 8 FSM case doesn't need BF to saturate DMA/RDMA throughput using u55c's HBM)

Second step is to move the generated verilog files to the correct position and configure coyote correctly.
1. Move the verilog under `hw/hdl/operators/examples/dedup`
2. Use corrent number of AXI in wrapper `hw/hdl/operators/examples/dedup/perf_dedup_c0_0.svh` and cmake configuration `hw/examples.cmake`

Last step, build HW(~4hour to get bit stream)

```Bash
cd coyote/hw
mkdir build && cd build

# use desired number memory channel: -DN_MEM_CHAN
# cmake options are under hw/examples.cmake, be sure to use correct mem channels: set(N_CARD_AXI ${NUM_FSM + 1}), where NUM_FSM is the number of FSM in hash table, +1 for the memory manager.
cmake .. -DFDEV_NAME=u55c -DEXAMPLE=perf_dedup

# use screen session on build server
screen
make shell && make compile

```

### Step 2 Software
Step 1, Generate network connection(routing) scheme. Configurations are generated with time stamp and the SW automatically use the latest one.
```Bash
./get_ip_remote.sh
./gen_network_config.sh
```
Note: for Chord routing mode, set routing mode register to 1 in `sw/src/dedupSys.cpp, L505` by using `cproc->setCSR(1, static_cast<uint32_t>(CTLR::ROUTING_MODE));`

Step 2, load bitstream and driver insertion
```Bash
# build driver if haven't done yet
cd driver && make
# prepare FPGA
./hw_reset_remote.sh
```

Step 3, run SW
```Bash
# build the correct software
cd sw
./sw_reset
# if you want to run on multiple nodes at once
cd ..
python3 run_experiment_node_sweep.py
```
### Raw measurement results and plots
The raw experiment results are under `measurement_results`, the plots and scripts for eurosys submission(also some other measurement results) are under `plot_eurosys_submission`

### Network Utilization
Current we are simulating network utilization, run `python3 simulate_network_utilization.py`

## CPU Baseline
CPU Baseline is adapted from dmdedup's hash table.
They are under `sw/examples/cpu_baseline*`, run:
```Bash
cd sw/examples && ./sw_reset.sh
./build/main -x xx
```