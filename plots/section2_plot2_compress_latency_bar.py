import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
import os

plt.rcParams.update({
  'font.size'        : 9, 
  'font.weight'      : 'bold', 
  'figure.facecolor' : 'w',
  'figure.dpi'       : 500,
  'figure.figsize'   : (6,2.5),
  # basic properties
  'axes.linewidth'   : 1,
  'xtick.top'        : True,
  'xtick.direction'  : 'in',
  'xtick.major.size' : '2',
  'xtick.major.width': '1',
  'ytick.right'      : True,
  'ytick.direction'  : 'in',
  'ytick.major.size' : '2',
  'ytick.major.width': '1', 
  'axes.grid'        : False,
  'grid.linewidth'   : '1',
  'legend.fancybox'  : False,
  'legend.framealpha': 1,
  'legend.edgecolor' : 'black',
  # case dependent
  'axes.autolimit_mode': 'round_numbers', # 'data' or 'round_numbers'
  'lines.linewidth'  : 1,
  'lines.markersize' : 6,
})

# Placeholder data
conditions = ['CPU Baseline', 'HW Baseline', 'StreamDedup']
cases = ['Write only\n 50% dup']
# ./main -n 16 -d 0.5 -f 0.5 -h 32 -c 32 -e 1
baseline_half_full = 1.621705e+03
cidr = (318 - 124)
stream_dedup_half_full = 24.010 + 25.16

cpu_hash = 1.11431e+03
cpu_compress = 1.93564e+03

pcie_round_trip = 3.5
hash_latency = 15
compression_latency = 25.16

cidr_transfer = pcie_round_trip * 2
cidr_hash = hash_latency
cidr_compress = compression_latency
cidr_lookup = 10
cidr_overhead = cidr - cidr_transfer - cidr_hash - cidr_compress - cidr_lookup

stream_dedup_transfer = pcie_round_trip
stream_dedup_hash = hash_latency
stream_dedup_compress = compression_latency
stream_dedup_lookup = stream_dedup_half_full - stream_dedup_transfer - stream_dedup_hash - stream_dedup_compress


latency = [a for a in [baseline_half_full, cidr, stream_dedup_half_full]]

# Number of cases
n_cases = len(cases)

# Creating the bar plot
fig, ax = plt.subplots()
bar_width = 0.3  # Width of the bars
bar_dist = 0.075
index = np.arange(n_cases)

# bar1 = ax.barh(1, latency[0], bar_width, label=conditions[0])
# bar2 = ax.barh(0, latency[1], bar_width, label=conditions[1])
# bar3 = ax.barh(-1, latency[2], bar_width, label=conditions[2])
ax.text(baseline_half_full*1.1, 1, f'{baseline_half_full:.0f}', fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'left', va = 'center')
ax.text(cidr*1.1, 0, f'{cidr:.0f}', fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'left', va = 'center')
ax.text(stream_dedup_half_full*1.1, -1, f'{stream_dedup_half_full:.0f}', fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'left', va = 'center')


ax.broken_barh([[0,latency[0]]], [1-bar_width/2, bar_width], label=conditions[0], facecolors='tab:blue', edgecolor='black')
# ax.broken_barh([[0,cidr_hash], 
#                 [cidr_hash, cidr_lookup], 
#                 [cidr_hash+cidr_lookup, cidr_compress],
#                 [cidr_hash+cidr_lookup+cidr_compress, cidr_transfer],
#                 [cidr_hash+cidr_lookup+cidr_compress+cidr_transfer, cidr_overhead]], [0-bar_width/2, bar_width], label=conditions[1], facecolors='tab:orange', edgecolor='black')
ax.broken_barh([[0,cidr_hash], 
                [cidr_hash, cidr_compress], 
                [cidr_hash+cidr_compress, cidr_transfer],
                [cidr_hash+cidr_compress+cidr_transfer, cidr_lookup + cidr_overhead]], [0-bar_width/2, bar_width], label=conditions[1], facecolors='tab:orange', edgecolor='black')
ax.broken_barh([[0,stream_dedup_hash], 
                [stream_dedup_hash, stream_dedup_compress], 
                [stream_dedup_hash+stream_dedup_compress, stream_dedup_transfer],
                [stream_dedup_hash+stream_dedup_compress+stream_dedup_transfer, stream_dedup_lookup]], [-1-bar_width/2, bar_width], label=conditions[2], facecolors='tab:green', edgecolor='black')
# print([[0,stream_dedup_hash], 
#                 [stream_dedup_hash, stream_dedup_hash+stream_dedup_lookup], 
#                 [stream_dedup_hash+stream_dedup_lookup, stream_dedup_hash+stream_dedup_lookup+stream_dedup_compress],
#                 [stream_dedup_hash+stream_dedup_lookup+stream_dedup_compress, stream_dedup_half_full]])

# Function to add labels on top of each bar
# def add_labels(bars):
#   for bar in bars:
#     height = bar.get_height()
#     ax.text(bar.get_x() + bar.get_width() / 2, height, f'{height:.01f}', fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# plt.text(3, -0.5, f"Hash", fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'center')
# plt.text(20, -0.5, f"(De)Compression", fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'center')
# plt.text(40, -1.2, , fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'center')
arrow_config = dict(facecolor='black', shrink=0.05, width=0.5, headwidth=2, headlength=2, linewidth=0.5)
ax.annotate(f"Hash", xy=(cidr_hash/3, 0), xytext=(1.5, -0.5), fontsize = 8,
                arrowprops=arrow_config, ha='center', va='center')
ax.annotate(f"Hash", xy=(cidr_hash/3, -1), xytext=(1.5, -0.5), fontsize = 8,
                arrowprops=arrow_config, ha='center', va='center')

ax.annotate(f"(De-)Compression", xy=(cidr_hash + cidr_compress/3, 0), xytext=(7, -0.5), fontsize = 8,
                arrowprops=arrow_config, ha='center', va='center')
ax.annotate(f"(De-)Compression", xy=(cidr_hash + cidr_compress/3, -1), xytext=(7, -0.5), fontsize = 8,
                arrowprops=arrow_config, ha='center', va='center')

ax.annotate(f"Data Movement", xy=(cidr_hash+cidr_compress + cidr_transfer/2, 0), xytext=(60, -0.5), fontsize = 8,
                arrowprops=arrow_config, ha='center', va='center')
ax.annotate(f"Data Movement", xy=(stream_dedup_hash+stream_dedup_compress + stream_dedup_transfer/2, -1), xytext=(60, -0.5), fontsize = 8,
                arrowprops=arrow_config, ha='center', va='center')

ax.annotate(f"Lookup & Overhead", xy=(cidr_hash+cidr_compress+cidr_transfer+(cidr_lookup + cidr_overhead)/5, 0), xytext=(1000, -0.5), fontsize = 8,
                arrowprops=arrow_config, ha='center', va='center')
ax.annotate(f"Lookup & Overhead", xy=(stream_dedup_hash+stream_dedup_compress+stream_dedup_transfer+ (stream_dedup_lookup)/5, -0.9), xytext=(1000, -0.5), fontsize = 8,
                arrowprops=arrow_config, ha='center', va='center')
# Add labels to each bar group
# add_labels(bar1)
# add_labels(bar2)
# add_labels(bar3)

# line for 12.7
# position_12GB7 = (12.7/4.096)
# plt.axhline(y = position_12GB7, color = 'r', linestyle = 'dashed', linewidth = 0.5, zorder = 5)
# plt.text(1.2 - 1, position_12GB7*1.02, f"12.7 GB/s", fontsize = 7, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# line for 3.5
# position_3GB5 = (3.1/4.096)
# plt.axhline(y = position_3GB5, color = 'r', linestyle = 'dashed', linewidth = 0.5, zorder = 5)
# plt.text(0.78 - 1, position_3GB5*1.02, f"3.1 GB/s", fontsize = 7, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# text for 12.5
# position_12GB5 = (12.5/4.096)
# plt.text(2 + (bar_dist + bar_width)/2, position_12GB7, f"12.5 GB/s", fontsize = 5, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# Adding labels and title
ax.set_xlabel('Latency [us]', fontsize = 9, weight = 'bold')
# ax.set_ylabel('Workload', fontsize = 9, weight = 'bold')
# ax.set_xticks(index)
ax.tick_params(axis='y', which='both', length=0, width=0)  # Adjust length and width as needed
ax.set_yticks((-1, 0, 1))
ax.set_yticklabels(['Stream\nDedup', 'HW\nBaseline', 'CPU\nBaseline'])

secax = ax.secondary_yaxis('left')
secax.set_yticks([idx - 0.5 for idx in range(-1, 2)])
secax.tick_params(axis='y', which='both', length=2, width=1)  # Adjust length and width as needed
secax.set_yticklabels([])  # Remove labels

legend = plt.legend()
ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.25), ncol=3, handlelength=0.7, handletextpad=0.5, markerfirst=True)
legend.get_frame().set_linewidth(1)
# legend.get_frame().set_edgecolor("b")
plt.xscale('log')
# plt.xlim([0 + (bar_dist + bar_width) / 2 - bar_width - 0.2, n_cases - 1 + (bar_dist + bar_width) / 2 + bar_width + 0.2])
# plt.xlim([0 - 0.5 , len(cases) - 1 + 0.5])
plt.xlim([1 ,10000])
# ax.yaxis.set_major_locator(ticker.MultipleLocator(2))
# ax.yaxis.set_minor_locator(ticker.MultipleLocator(0.5))

# Adjusting the layout
plt.subplots_adjust(left=0.12, right=0.98, top=0.85, bottom=0.18)
# Alternatively
# plt.tight_layout()

# Function to save the plot in a given directory in both PNG and PDF formats
def save_plot(directory, filename):
    # Create the directory if it doesn't exist
    os.makedirs(directory, exist_ok=True)
    plt.savefig(f"{directory}/{filename}.png")
    plt.savefig(f"{directory}/{filename}.pdf")

# Example usage (replace 'your_directory_path' with the actual path)
save_plot('./section2', 'plot2_compress_latency_bar')


# marker_lst      = ['o','x','+','v','^','s','d']
# # marker_size_lst =  [1,  2,  3,  1,  1,  1,  1]
# marker_size_lst =  [2,  3,  4,  2,  2,  2,  2]
# marker_idx = 2
# save_format_lst = ['pdf', 'png']
# case_lst = ['Insertion', 'Deletion']
# case_file_lst = ['insertion_6FSM_BF', 'deletion_6FSM_BF']

# case_lst.reverse()
# case_file_lst.reverse()

# fig, ax1 = plt.subplots()

# for case_idx in range(len(case_lst)):
#   case_label = case_lst[case_idx]
#   case_file = case_file_lst[case_idx]
#   latency_data_csv = pd.read_csv('./plot1/'+ case_file + '_latency.csv')
#   latency_data = latency_data_csv.iloc[:, [1]].to_numpy().reshape([-1])
#   fullness_data = [0.1 + idx * 0.1 for idx in range(10)]
  
#   # data extracted, plot
#   # plt.plot(fullness_data, latency_data, label = f"Hash Table Fullness = {fullness:.0%}", marker = marker_lst[marker_idx], markersize = marker_size_lst[marker_idx])
#   plt.plot(fullness_data, latency_data, 
#            linewidth=2, 
#            label = case_label, 
#            marker = marker_lst[marker_idx], 
#            markersize = marker_size_lst[marker_idx]+2,
#            markeredgewidth = 2)
#   marker_idx = marker_idx - 1
# print(max_freq_lst)
# print(power_at_max_freq_lst)

# if case_name == 'mmfp32':
#   plt.plot([370, 340, 310, 280, 250, 220, 180],
#            [35.296729199999994, 29.6508939, 24.7147813, 20.34978855, 16.48387, 13.0884749, 9.666480600000002],
#            '-.',linewidth = 1, alpha = 1)
#   plt.text(250, 14, "Max Frequency", fontsize = 7, rotation=40, rotation_mode='anchor')
#   plt.title("MATMUL FP32 Power vs. Frequency", weight = 'bold')
# elif case_name == 'mmint32':
#   plt.plot([370, 340, 310, 280, 250, 220, 180],
#            [29.155039200000004, 24.556644599999995, 20.453543, 16.86332025, 13.600857, 10.78313555, 7.947639899999999],
#            '-.',linewidth = 1, alpha = 1)
#   plt.text(250, 11, "Max Frequency", fontsize = 7, rotation=35, rotation_mode='anchor', weight = 'bold')
#   plt.title("MATMUL INT32 Power vs. Frequency",  weight = 'bold')

# legend = plt.legend()
# legend.get_frame().set_linewidth(1)
# # legend.get_frame().set_edgecolor("b")

# plt.xlim([0, 1])
# plt.ylim([0,35])

# plt.axhline(y = ((12/4)/1.024), color = 'r', linestyle = 'dashed')
# plt.text(1.01, 2.85, f"12 GB/s", fontsize = 6, rotation=0, rotation_mode='anchor', weight = 'bold')
# plt.text(0.45, 2.6, f"12 GB/s", fontsize = 6, rotation=0, rotation_mode='anchor', weight = 'bold')

# ax1.set_xticklabels(['{:,.0%}'.format(x) for x in [0 + idx * 0.2 for idx in range(6)]])

# # plt.xticks(x, weight = 'bold')
# plt.xlabel("Hash Table Fullness", fontsize = 9, weight = 'bold')
# plt.ylabel("Latency[us]", fontsize = 10, weight = 'bold')


# for save_format in save_format_lst:
#   plt.savefig("./plot1/" + "latency_fullness_6FSM_BF_big." + save_format)