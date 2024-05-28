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
23988.0,
28335.0,
28564.899999999998,
28526.175,
28749.2,
32651.18333333333,
33311.41428571429,
33190.2375,
33158.177777777775,
 33066.090000000004,
]

condition_2 = [
24032.8,
28368.5,
28966.63333333333,
29209.225,
29123.7,
33354.6,
33689.200000000004,
33987.875,
32489.399999999998,
 33483.39,
]

condition_3 = [
7774.75,
11628.4,
10765.566666666668,
11087.875,
10976.34,
14955.533333333335,
15812.442857142858,
15592.775,
15495.800000000001,
 15642.87,
]

condition_4 = [
7982.25,
11325.25,
11125.166666666666,
12616.075,
11009.439999999999,
15446.133333333331,
15662.614285714286,
15803.8125,
15600.31111111111,
 15676.55,
]

condition_5 = [
7158.5,
11135.0,
11045.066666666666,
10972.05,
10758.52,
15337.166666666666,
16085.228571428572,
15460.887499999999,
15158.199999999999,
 15505.929999999998,
]


throughput_1 = [(condition_1[idx]/1000) for idx in range(len(condition_1))]
throughput_2 = [(condition_2[idx]/1000) for idx in range(len(condition_2))]
throughput_3 = [(condition_3[idx]/1000) for idx in range(len(condition_3))]
throughput_4 = [(condition_4[idx]/1000) for idx in range(len(condition_4))]
throughput_5 = [(condition_5[idx]/1000) for idx in range(len(condition_5))]
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
plt.axhline(y = 15.5, color = 'r', linestyle = 'dashed', linewidth = 0.5, zorder = 5)
plt.text(5.5, 15.5*1.02, f"15.5 us", fontsize = 7, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

plt.axhline(y = 33.5, color = 'r', linestyle = 'dashed', linewidth = 0.5, zorder = 5)
plt.text(5.5, 33.5*1.02, f"33.5 us", fontsize = 7, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# Adding labels and title
ax.set_xlabel('Number of FPGA nodes', fontsize = 9, weight = 'bold')
ax.set_ylabel('Latency [us]', fontsize = 9, weight = 'bold')
ax.set_xticks(node_lst)
# ax.tick_params(axis='x', which='both', length=0, width=0)  # Adjust length and width as needed
# ax.set_xticklabels(cases)

legend = plt.legend()
ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.45), ncol=3)
legend.get_frame().set_linewidth(1)
# legend.get_frame().set_edgecolor("b")

# plt.xlim([0 + (bar_dist + bar_width) / 2 - bar_width - 0.2, n_cases - 1 + (bar_dist + bar_width) / 2 + bar_width + 0.2])
plt.xlim([1, 10])
# plt.ylim([0, 40])
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
save_plot('./plot6', 'latency_node_half_line')
