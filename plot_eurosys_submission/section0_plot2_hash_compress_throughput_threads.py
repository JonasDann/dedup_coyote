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
cases = ['SHA3 256', 'GZIP']
thread_lst = [2**idx for idx in range(0, 8)]

cpu_sha_time = {
1   :1.82695e+08,
2   :9.4236e+07,
3   :6.35018e+07,
4   :4.76467e+07,
5   :3.86245e+07,
6   :3.26759e+07,
7   :2.85704e+07,
8   :2.51871e+07,
9   :2.28814e+07,
10  :2.07834e+07,
16  :2.3635e+07,
24  :1.80278e+07,
32  :1.85127e+07,
64  :1.99739e+07,
128 : 2.11062e+07,
256 : 2.18082e+07,
}
cpu_gzip_time = {
1 : 1.69502e+09,
2 : 1.00234e+09,
3 : 7.47933e+08,
4 : 5.87023e+08,
5 : 5.22069e+08,
6 : 4.56588e+08,
7 : 4.30161e+08,
8 : 4.21006e+08,
9 : 4.41823e+08,
10 : 4.55639e+08,
16 : 5.44741e+08,
24 : 4.71225e+08,
32 : 4.91928e+08,
64 : 6.68812e+08,
128 : 7.72544e+08,
256 : 8.37113e+08,
}
hash_core_throughput = 4.096/15 # GB/s
cpu_sha_throughput = [(16384*4096/(cpu_sha_time[n_thread])) for n_thread in thread_lst]
cpu_gzip_throughput = [(16384*4096/(cpu_gzip_time[n_thread])) for n_thread in thread_lst]
# fpga_throughput = [min(n_thread*hash_core_throughput, 12.8) for n_thread in thread_lst]
print(cpu_sha_throughput) # [0.36732731601850077, 0.7121361687677745, 1.4084682464892637, 2.6644140849879503, 2.839384979902687, 3.6250176365414015, 3.3598277752466967, 3.1795805971705]
print(cpu_gzip_throughput) # [0.03959178298781135, 0.06695219586168366, 0.11432067227348844, 0.15940120568352945, 0.12319407571671676, 0.13642009399749558, 0.10034040059089849, 0.08686736807223926]
# Number of cases
n_cases = len(cases)

# Creating the bar plot
fig, ax = plt.subplots()

# line1, = plt.plot(node_lst, throughput_1, label=cases[0], marker='o', zorder=3, color = '#1f77b4',)
# line2, = plt.plot(node_lst, throughput_2, label=cases[1], marker='x', zorder=3, color = '#1f77b4',)
# line3, = plt.plot(node_lst, throughput_3, label=cases[2], marker='+', zorder=3, color = '#ff7f0e',)
# line4, = plt.plot(node_lst, throughput_4, label=cases[3], marker='v', zorder=3, color = '#ff7f0e',)
# line5, = plt.plot(node_lst, throughput_5, label=cases[4], marker='^', zorder=3, color = '#2ca02c',)

line1, = plt.plot(thread_lst, cpu_sha_throughput, label=cases[0], marker='v', markersize = 6, zorder=3)
line2, = plt.plot(thread_lst, cpu_gzip_throughput, label=cases[1], marker='d', markersize = 6, zorder=3)

line1.set_clip_on(False)
line2.set_clip_on(False)

# line for 12.7
# plt.axhline(y = 12.8, color = 'r', linestyle = 'dashed', linewidth = 0.5, zorder = 5)
# plt.text(2**3.5, 11.6, f"Max Throughput: 12.8 GB/s", fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# plt.axhline(y = 59.6, color = 'r', linestyle = 'dashed', linewidth = 0.5, zorder = 5)
plt.text(32, 4.2, f"3.6 GB/s", fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')
plt.text(8, 0.2, f"0.16 GB/s", fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# Adding labels and title
ax.set_xlabel('Number of CPU Threads', fontsize = 9, weight = 'bold')
ax.set_ylabel('Throughput [GB/s]', fontsize = 9, weight = 'bold')
# ax.set_xticks(node_lst)
# ax.tick_params(axis='x', which='both', length=0, width=0)  # Adjust length and width as needed
# ax.set_xticklabels(cases)

legend = plt.legend()
# ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.35), ncol=2)
ax.legend(loc='upper left')
legend.get_frame().set_linewidth(1)
# legend.get_frame().set_edgecolor("b")

# plt.xlim([0 + (bar_dist + bar_width) / 2 - bar_width - 0.2, n_cases - 1 + (bar_dist + bar_width) / 2 + bar_width + 0.2])
plt.xscale('log', base=2)
plt.xlim([1, 128])
# Set the ticks to be in powers of 2
plt.xticks(thread_lst, thread_lst)
# plt.ylim([0, 5])
plt.yscale('log')
# ax.yaxis.set_major_locator(ticker.MultipleLocator(2))
# ax.yaxis.set_minor_locator(ticker.MultipleLocator(0.5))

# Adjusting the layout
plt.subplots_adjust(left=0.1, right=0.95, top=0.95, bottom=0.17)
# Alternatively
# plt.tight_layout()

# Function to save the plot in a given directory in both PNG and PDF formats
def save_plot(directory, filename):
    # Create the directory if it doesn't exist
    os.makedirs(directory, exist_ok=True)
    plt.savefig(f"{directory}/{filename}.png")
    plt.savefig(f"{directory}/{filename}.pdf")

# Example usage (replace 'your_directory_path' with the actual path)
save_plot('./section0', 'plot2_hash_compress_throughput_threads')
