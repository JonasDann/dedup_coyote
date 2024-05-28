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
cases = ['Write only\n all dup', 'Write only\n half dup', 'Write only\n no dup', 'Erase only\n all GC', 'Erase only\n no GC', 'Read only']
conditions = ['CPU Baseline', 'StreamDedup']
# insertion: 16 thread, slightly faster than 32 thread
# ./main -n 16384 -d 0 -f 0.99 -h 16
# ./main -n 16384 -g 0 -f 0.5 -o 2
# baseline_half_full = [1.91277e+07, 1.91465e+07, 2.078865e+07, 5.6665225e+06 ,5.538165e+06, 3.27925e+06]
# baseline_full = [1.957e+07, 6.285585e+09, 1.5509775e+10, 9.6264575e+06, 7.38419e+06, 5.4514425e+06]

# SHA3-256
# insertion: 64 thread, slightly faster than 32/128 thread
# ./main -n 16384 -d 0 -f 0.99 -h 64
# ./main -n 16384 -g 0 -f 0.5 -o 2
baseline_half_full = [2.13893e+07, 2.1285775e+07, 2.212275e+07, 5.6665225e+06 ,5.538165e+06, 3.27925e+06]
baseline_full = [2.1628575e+07, 6.2590075e+09, 1.5430925e+10, 9.6264575e+06, 7.38419e+06, 5.4514425e+06]

stream_dedup_half_full = [5288480.0, 5285780.0, 5285320.0, 2991200.0, 3060070.0, 2759910.0]
stream_dedup_full = [5287290.0, 5286860.0, 5358210.0, 4310450.0, 4382740.0, 4072770.0]

throughput_1 = [16384/(a/1000) for a in baseline_full]
throughput_2 = [16384/(a/1000) for a in stream_dedup_full]
improvement = [b/a for a, b in zip(throughput_1, throughput_2)]
print(f'{improvement=}') 
# SHA2 improvement=[3.701329036235955, 1188.9070260986673, 2894.581399385242, 2.2332836478789924, 1.6848341448500255, 1.3385097857232302]
# SHA3 improvement=[4.090673104747423, 1183.8799400778535, 2879.865664093046, 2.2332836478789924, 1.6848341448500255, 1.3385097857232302]
# Number of cases
n_cases = len(cases)

# Creating the bar plot
fig, ax = plt.subplots()
bar_width = 0.25  # Width of the bars
bar_dist = 0.05
index = np.arange(n_cases)

bar1 = ax.bar(index - (bar_dist + bar_width)/2, throughput_1, bar_width, label=conditions[0])
bar2 = ax.bar(index + (bar_dist + bar_width)/2, throughput_2, bar_width, label=conditions[1])

# line for 12.7
position_12GB7 = (12.7/4.096)
plt.axhline(y = position_12GB7, color = 'r', linestyle = 'dashed', linewidth = 0.5, zorder = 5)
plt.text(1.2 - 1, position_12GB7*1.02, f"12.7 GB/s", fontsize = 7, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# line for 3.5
position_3GB5 = (3.1/4.096)
plt.axhline(y = position_3GB5, color = 'r', linestyle = 'dashed', linewidth = 0.5, zorder = 5)
plt.text(0.77 - 1, position_3GB5*1.02, f"3.1 GB/s", fontsize = 7, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# text for 12.5
# position_12GB5 = (12.5/4.096)
# plt.text(2 + (bar_dist + bar_width)/2, position_12GB7, f"12.5 GB/s", fontsize = 5, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# Adding labels and title
ax.set_ylabel('Throughput [M Pages/s]', fontsize = 9, weight = 'bold')
# ax.set_xlabel('Workload', fontsize = 9, weight = 'bold')
ax.set_xticks(index)
ax.tick_params(axis='x', which='both', length=0, width=0)  # Adjust length and width as needed
ax.set_xticklabels(cases)

secax = ax.secondary_xaxis('bottom')
secax.set_xticks([idx - 0.5 for idx in range(0, n_cases + 1)])
secax.tick_params(axis='x', which='both', length=2, width=1)  # Adjust length and width as needed
secax.set_xticklabels([])  # Remove labels

legend = plt.legend()
ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.25), ncol=2, handlelength=0.7, handletextpad=0.5, markerfirst=True)
legend.get_frame().set_linewidth(1)
# legend.get_frame().set_edgecolor("b")

# plt.xlim([0 + (bar_dist + bar_width) / 2 - bar_width - 0.2, n_cases - 1 + (bar_dist + bar_width) / 2 + bar_width + 0.2])
plt.xlim([0 - 0.5 , 5 + 0.5])
plt.ylim([0, 5])
ax.yaxis.set_major_locator(ticker.MultipleLocator(2))
# ax.yaxis.set_minor_locator(ticker.MultipleLocator(0.5))

# Adjusting the layout
plt.subplots_adjust(left=0.1, right=0.99, top=0.85, bottom=0.17)
# Alternatively
# plt.tight_layout()

# Function to save the plot in a given directory in both PNG and PDF formats
def save_plot(directory, filename):
    # Create the directory if it doesn't exist
    os.makedirs(directory, exist_ok=True)
    plt.savefig(f"{directory}/{filename}.png")
    plt.savefig(f"{directory}/{filename}.pdf")

# Example usage (replace 'your_directory_path' with the actual path)
save_plot('./section1', 'plot2_throughput_workload_bar_full')


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