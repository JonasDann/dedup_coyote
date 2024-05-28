import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
import os

plt.rcParams.update({
  'font.size'        : 9, 
  'font.weight'      : 'bold', 
  'figure.facecolor' : 'w',
  'figure.dpi'       : 500,
  'figure.figsize'   : (6,3),
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
cases = ['Write only, all dup', 'Write only, no dup', 'Erase only, all GC', 'Erase only, no GC']
node_lst = [idx for idx in range(1, 11)]

condition_1 = [
25398.0,
29819.0,
29399.86666666667,
29468.425,
29592.52,
33318.56666666667,
33640.4,
33966.8875,
33760.12222222222,
 33862.28,
]

condition_2 = [
25533.5,
30264.75,
30669.066666666666,
30246.75,
30461.920000000002,
34054.700000000004,
34750.62857142858,
34605.3,
34249.21111111111,
 34363.009999999995,
]

condition_3 = [
9952.0,
12467.35,
12021.133333333333,
12153.175,
12318.560000000001,
16126.6,
16462.27142857143,
16119.0625,
16310.255555555554,
 16501.769999999997,
]

condition_4 = [
9785.25,
12724.15,
12313.966666666667,
12325.9,
12538.76,
16802.966666666667,
16448.314285714285,
16450.7,
16599.555555555555,
 16636.45,
]

compression_latency = 25.16 #us

throughput_1 = [(condition_1[idx]/1000 + compression_latency) for idx in range(len(condition_1))]
throughput_2 = [(condition_2[idx]/1000 + compression_latency) for idx in range(len(condition_2))]
throughput_3 = [(condition_3[idx]/1000) for idx in range(len(condition_3))]
throughput_4 = [(condition_4[idx]/1000) for idx in range(len(condition_4))]
# Number of cases
n_cases = len(cases)

# Creating the bar plot
fig, ax = plt.subplots()

# line1, = plt.plot(node_lst, throughput_1, label=cases[0], marker='o', zorder=3, color = '#1f77b4',)
# line2, = plt.plot(node_lst, throughput_2, label=cases[1], marker='x', zorder=3, color = '#1f77b4',)
# line3, = plt.plot(node_lst, throughput_3, label=cases[2], marker='+', zorder=3, color = '#ff7f0e',)
# line4, = plt.plot(node_lst, throughput_4, label=cases[3], marker='v', zorder=3, color = '#ff7f0e',)
# line5, = plt.plot(node_lst, throughput_5, label=cases[4], marker='^', zorder=3, color = '#2ca02c',)

line1, = plt.plot(node_lst, throughput_1, label=cases[0], marker='s', markersize = 10, zorder=3, linewidth = 2)
line2, = plt.plot(node_lst, throughput_2, label=cases[1], marker='d', markersize = 9, zorder=3, linewidth = 2)
line3, = plt.plot(node_lst, throughput_3, label=cases[2], marker='s', markersize = 10, zorder=3, linewidth = 2)
line4, = plt.plot(node_lst, throughput_4, label=cases[3], marker='d', markersize = 9, zorder=3, linewidth = 2)

line1.set_clip_on(False)
line2.set_clip_on(False)
line3.set_clip_on(False)
line4.set_clip_on(False)

# line for 12.7
plt.axhline(y = 16.5, color = 'r', linestyle = 'dashed', linewidth = 1, zorder = 5)
plt.text(5.4, 16.5*1.02, f"16.5 us", fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

plt.axhline(y = 59.6, color = 'r', linestyle = 'dashed', linewidth = 1, zorder = 5)
plt.text(5.4, 59.6*1.02, f"59.6 us", fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# Adding labels and title
ax.set_xlabel('Number of FPGA nodes', fontsize = 9, weight = 'bold')
ax.set_ylabel('Latency [us]', fontsize = 9, weight = 'bold')
ax.set_xticks(node_lst)
# ax.tick_params(axis='x', which='both', length=0, width=0)  # Adjust length and width as needed
# ax.set_xticklabels(cases)

legend = plt.legend()
ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.3), ncol=2)
legend.get_frame().set_linewidth(1)
# legend.get_frame().set_edgecolor("b")

# plt.xlim([0 + (bar_dist + bar_width) / 2 - bar_width - 0.2, n_cases - 1 + (bar_dist + bar_width) / 2 + bar_width + 0.2])
plt.xlim([1, 10])
# plt.ylim([0, 40])
# ax.yaxis.set_major_locator(ticker.MultipleLocator(2))
# ax.yaxis.set_minor_locator(ticker.MultipleLocator(0.5))

# Adjusting the layout
plt.subplots_adjust(left=0.1, right=0.95, top=0.8, bottom=0.17)
# Alternatively
# plt.tight_layout()

# Function to save the plot in a given directory in both PNG and PDF formats
def save_plot(directory, filename):
    # Create the directory if it doesn't exist
    os.makedirs(directory, exist_ok=True)
    plt.savefig(f"{directory}/{filename}.png")
    plt.savefig(f"{directory}/{filename}.pdf")

# Example usage (replace 'your_directory_path' with the actual path)
save_plot('./section3', 'plot2_latency_node_line_full')
