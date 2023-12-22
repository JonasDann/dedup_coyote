#!/usr/bin/env python3

import os

def dump_xdc(f, pblk_type, pblk_name, pblk_loc_range, pblk_cell):
	f.write("# " + pblk_name + "\n")
	# create block
	f.write("create_pblock " + pblk_name + "\n")
	# resize block
	f.write("resize_pblock " + pblk_name + " -add " + pblk_type + "_X" + str(pblk_loc_range[0]) + "Y" + str(pblk_loc_range[1]) + ":" + pblk_type + "_X" + str(pblk_loc_range[2]) + "Y" + str(pblk_loc_range[3]) + "\n")
	# add cells
	for cell in pblk_cell:
		f.write("add_cells_to_pblock " + pblk_name + " [get_cells [list " + cell + "]]\n")
	f.write("\n")

# hw config (same name as hw/cmake)
n_hbm_chan = 16
en_rdma_0 = 1
en_rdma_1 = 0
en_tcp_0 = 0
en_tcp_1 = 0

# pre-define all pblocks
pblk_slice_loc = {}
pblk_bram_loc = {}
pblk_clkreg_loc = {}

pblk_cell_list = {}
# hbm pblk: each 4 channels share one clock region
for ii in range(0, int((n_hbm_chan+3)/4)):
	pblk_name = "pblk_hbm" + str(ii)
	pblk_slice_loc[pblk_name] = (int(ii*29) + int(ii/4), 0, int((ii+1)*29-1), 59) # tuple slice(x,y,x,y)
	pblk_cell_list[pblk_name] = []

# network (clock region)
pblk_clkreg_loc["pblk_net_module_0"] = (0, 6, 0, 7)
pblk_cell_list["pblk_net_module_0"] = []
pblk_clkreg_loc["pblk_net_early_ccross_0"] = (0, 4, 0, 5)
pblk_cell_list["pblk_net_early_ccross_0"] = []
pblk_clkreg_loc["pblk_net_stack_0"] = (1, 4, 3, 7) # RDMA
pblk_cell_list["pblk_net_stack_0"] = []

# add cells to pblks following the hw config
# hbm path
for ii in range(0, n_hbm_chan):
	pblk_name = "pblk_hbm" + str(int(ii/4))
	path_cell_name = "inst_int_hbm/path_" + str(ii)
	pblk_cell_list[pblk_name].append(path_cell_name)

# network module
pblk_cell_list["pblk_net_module_0"] = ["inst_network_top_0/inst_network_module"]
pblk_cell_list["pblk_net_early_ccross_0"] = ["inst_network_top_0/inst_early_ccross"]
pblk_cell_list["pblk_net_stack_0"] = ["inst_network_top_0/inst_network_stack"]


file_name = "u55c_placement.xdc"
with open(file_name, 'w') as f:
	for pblk_name in pblk_slice_loc:
		dump_xdc(f, "SLICE", pblk_name, pblk_slice_loc[pblk_name], pblk_cell_list[pblk_name])

	for pblk_name in pblk_clkreg_loc:
		dump_xdc(f, "CLOCKREGION", pblk_name, pblk_clkreg_loc[pblk_name], pblk_cell_list[pblk_name])

	f.close()