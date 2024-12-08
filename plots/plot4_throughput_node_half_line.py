import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
import os

plt.rcParams.update({
  'font.size'        : 9, 
  'font.weight'      : 'bold', 
  'figure.facecolor' : 'w',
  'figure.dpi'       : 500,
  'figure.figsize'   : (6,2),
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
cases = ['Write only, all dup', 'Write only, no dup', 'Erase only, all GC', 'Erase only, no GC', 'Read only']
node_lst = [idx for idx in range(1, 11)]

condition_1 = [
5288480.0,
5305265.0,
5299493.333333333,
5313477.5,
5303464.0,
5299468.333333333,
5311231.428571428,
5311047.5,
5302362.222222222,
 5310759.0,
]

condition_2 = [
5285320.0,
5292890.0,
5293123.333333333,
5292192.5,
5290704.0,
5293401.666666667,
5300115.714285715,
5295585.0,
5295711.111111111,
 5296307.0,
]

condition_3 = [
2991200.0,
3704850.0,
3068243.3333333335,
2886452.5,
2897512.0,
3505226.6666666665,
3900732.8571428573,
3790323.75,
3722976.6666666665,
 4137784.0,
]

condition_4 = [
3060070.0,
3697950.0,
3130763.3333333335,
2961335.0,
3000282.0,
3494520.0,
3891992.8571428573,
3839651.25,
3740395.5555555555,
 4161716.0,
]

condition_5 = [
2759910.0,
3728480.0,
3063036.6666666665,
2725830.0,
2780458.0,
3519880.0,
3920027.1428571427,
3815536.25,
3743075.5555555555,
 4179198.0,
]


throughput_1 = [16384/(condition_1[idx]/1000) * (idx + 1) for idx in range(len(condition_1))]
throughput_2 = [16384/(condition_2[idx]/1000) * (idx + 1) for idx in range(len(condition_2))]
throughput_3 = [16384/(condition_3[idx]/1000) * (idx + 1) for idx in range(len(condition_3))]
throughput_4 = [16384/(condition_4[idx]/1000) * (idx + 1) for idx in range(len(condition_4))]
throughput_5 = [16384/(condition_5[idx]/1000) * (idx + 1) for idx in range(len(condition_5))]
# Number of cases
n_cases = len(cases)

# Creating the bar plot
fig, ax = plt.subplots()

# line1, = plt.plot(node_lst, throughput_1, label=cases[0], marker='o', zorder=3, color = '#1f77b4',)
# line2, = plt.plot(node_lst, throughput_2, label=cases[1], marker='x', zorder=3, color = '#1f77b4',)
# line3, = plt.plot(node_lst, throughput_3, label=cases[2], marker='+', zorder=3, color = '#ff7f0e',)
# line4, = plt.plot(node_lst, throughput_4, label=cases[3], marker='v', zorder=3, color = '#ff7f0e',)
# line5, = plt.plot(node_lst, throughput_5, label=cases[4], marker='^', zorder=3, color = '#2ca02c',)

line1, = plt.plot(node_lst, throughput_1, label=cases[0], marker='v', markersize = 5, zorder=3)
line2, = plt.plot(node_lst, throughput_2, label=cases[1], marker='v', markersize = 5, zorder=3)
line3, = plt.plot(node_lst, throughput_3, label=cases[2], marker='d', markersize = 5, zorder=3)
line4, = plt.plot(node_lst, throughput_4, label=cases[3], marker='d', markersize = 5, zorder=3)
line5, = plt.plot(node_lst, throughput_5, label=cases[4], marker='o', markersize = 5, zorder=3)

line1.set_clip_on(False)
line2.set_clip_on(False)
line3.set_clip_on(False)
line4.set_clip_on(False)
line5.set_clip_on(False)

# line for 12.7
position_127GB = (127/4.096)
plt.axhline(y = position_127GB, color = 'r', linestyle = 'dashed', linewidth = 0.5, zorder = 5)
plt.text(5.5, position_127GB*1.02, f"127 GB/s", fontsize = 7, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# Adding labels and title
ax.set_xlabel('Number of FPGA nodes', fontsize = 9, weight = 'bold')
ax.set_ylabel('Throughput [M Pages/s]', fontsize = 9, weight = 'bold')
ax.set_xticks(node_lst)
# ax.tick_params(axis='x', which='both', length=0, width=0)  # Adjust length and width as needed
# ax.set_xticklabels(cases)

legend = plt.legend()
ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.45), ncol=3)
legend.get_frame().set_linewidth(1)
# legend.get_frame().set_edgecolor("b")

# plt.xlim([0 + (bar_dist + bar_width) / 2 - bar_width - 0.2, n_cases - 1 + (bar_dist + bar_width) / 2 + bar_width + 0.2])
plt.xlim([1, 10])
plt.ylim([0, 40])
# ax.yaxis.set_major_locator(ticker.MultipleLocator(2))
# ax.yaxis.set_minor_locator(ticker.MultipleLocator(0.5))

# Adjusting the layout
plt.subplots_adjust(left=0.1, right=0.95, top=0.75, bottom=0.17)
# Alternatively
# plt.tight_layout()

# Function to save the plot in a given directory in both PNG and PDF formats
def save_plot(directory, filename):
    # Create the directory if it doesn't exist
    os.makedirs(directory, exist_ok=True)
    plt.savefig(f"{directory}/{filename}.png")
    plt.savefig(f"{directory}/{filename}.pdf")

# Example usage (replace 'your_directory_path' with the actual path)
save_plot('./plot4', 'throughput_node_half_line')


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