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
data = {'node_count': [1, 2, 3, 4, 5, 6, 7, 8, 9, 10], 
        'max_hop_count': [0, 2, 2, 2, 2, 4, 4, 4, 4, 4],
        'avg_hop_count': [0.0, 1.0, 1.3333333333333333, 1.5, 1.6, 2.0, 2.2857142857142856, 2.25, 2.2222222222222223, 2.4]}
data_log = {'node_count': [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024], 
            'max_hop_count': [0, 2, 2, 4, 5, 6, 7, 8, 9, 10, 11], 
            'avg_hop_count': [0.0, 1.0, 1.5, 2.25, 3.0, 3.8125, 4.625, 5.453125, 6.28125, 7.11328125, 7.9453125]}
data=data_log
latency_data = [a for a in data['max_hop_count']]
util_data = [100*a*512/(4096*8)/2 for a in data['avg_hop_count']]
# Creating the bar plot
fig, ax1 = plt.subplots()
line1, = ax1.plot(data['node_count'], latency_data, label='Latency', marker='s', markersize = 6, zorder=3, color = 'tab:blue')
ax1.set_xlabel('Number of FPGA Nodes', fontsize = 9, weight = 'bold')
ax1.set_ylabel('Latency [#Hops]', color='tab:blue',fontsize = 9, weight = 'bold')
ax1.tick_params(axis='y', labelcolor='tab:blue')

ax2 = ax1.twinx()
line2, = ax2.plot(data['node_count'], util_data, label='Duplex Bandwidth Utilization', marker='d', markersize=6, zorder=3, color='tab:orange')
ax2.set_ylabel('Duplex Bandwidth Utilization[%]', color='tab:orange',fontsize = 9, weight = 'bold')
ax2.tick_params(axis='y', labelcolor='tab:orange')

line1.set_clip_on(False)
line2.set_clip_on(False)

# line for 12.7
# plt.axhline(y = 12.8, color = 'r', linestyle = 'dashed', linewidth = 0.5, zorder = 5)
# plt.text(2**3.5, 11.6, f"Max Throughput: 12.8 GB/s", fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# plt.axhline(y = 59.6, color = 'r', linestyle = 'dashed', linewidth = 0.5, zorder = 5)
# plt.text(32, 4, f"3.6 GB/s", fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# Adding labels and title

# ax.set_ylabel('Throughput [GB/s]', fontsize = 9, weight = 'bold')
# ax.set_xticks(node_lst)
# ax.tick_params(axis='x', which='both', length=0, width=0)  # Adjust length and width as needed
# ax.set_xticklabels(cases)
# Add legends
lines = [line1, line2]
labels = [line.get_label() for line in lines]
# legend=ax1.legend(lines, labels, loc='upper left')
legend=ax1.legend(lines, labels,loc='upper center', bbox_to_anchor=(0.5, 1.2), ncol=2)
# plt.legend(loc='upper left')
legend.get_frame().set_linewidth(1)
# legend.get_frame().set_edgecolor("b")

# plt.xlim([0 + (bar_dist + bar_width) / 2 - bar_width - 0.2, n_cases - 1 + (bar_dist + bar_width) / 2 + bar_width + 0.2])
plt.xscale('log', base=2)
plt.xlim([data['node_count'][0], data['node_count'][-1]])
# Set the ticks to be in powers of 2
plt.xticks(data['node_count'], data['node_count'])
ax1.set_ylim([0, 14])
ax2.set_ylim([0, 14])
ax1.yaxis.set_major_locator(ticker.MultipleLocator(2))
ax2.yaxis.set_major_locator(ticker.MultipleLocator(2))
# ax.yaxis.set_minor_locator(ticker.MultipleLocator(0.5))

# Adjusting the layout
plt.subplots_adjust(left=0.07, right=0.93, top=0.85, bottom=0.17)
# Alternatively
# plt.tight_layout()

# Function to save the plot in a given directory in both PNG and PDF formats
def save_plot(directory, filename):
    # Create the directory if it doesn't exist
    os.makedirs(directory, exist_ok=True)
    plt.savefig(f"{directory}/{filename}.png")
    plt.savefig(f"{directory}/{filename}.pdf")

# Example usage (replace 'your_directory_path' with the actual path)
save_plot('./section4', 'plot2_hop_count_util_node_log')
