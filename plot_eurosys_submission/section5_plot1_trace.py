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
rounds = 35
cases = ['mail', 'web', 'homes']
pages = [p * 16384 for p in list(range(rounds))]

kiops_1 = [
0 for _ in list(range(rounds))
]

kiops_2 = [
2327.101, 2254.741, 2602.5270000000005, 3052.4439999999995, 2540.199, 2360.113, 2828.974, 3155.231, 3215.958, 3253.603, 3047.811, 3274.3860000000004, 3144.165, 3152.599, 3210.616, 3050.8480000000004, 2966.564, 2709.527, 2629.5970000000007, 3012.7840000000006, 3395.785, 3027.09, 3050.727, 3051.81, 3071.772, 3512.44, 2793.993, 2755.1040000000003, 2919.08, 3022.948, 2617.191, 1952.884, 2816.5190000000002, 3224.435
]

kiops_3 = [
0 for _ in list(range(rounds))
]

# Number of cases
n_cases = len(cases)

# Creating the bar plot
fig, ax = plt.subplots()

# default color: ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', '#8c564b', '#e377c2', '#7f7f7f', '#bcbd22', '#17becf']
line1, = plt.plot(pages, kiops_1, label=cases[0], marker='s', markersize = 10, zorder=3, linestyle='-', linewidth = 2)
line2, = plt.plot(pages, kiops_2, label=cases[1], marker='d', markersize = 9, zorder=3, linestyle='-', linewidth = 2)
line3, = plt.plot(pages, kiops_3, label=cases[2], marker='x', markersize = 8, zorder=3, markeredgewidth=2, linewidth = 2)
line1.set_clip_on(False)
line2.set_clip_on(False)
line3.set_clip_on(False)

# line for 12.7
# position_127GB = 124.3
# plt.axhline(y = position_127GB, color = 'r', linestyle = 'dashed', linewidth = 1, zorder = 5)
# plt.text(5.5, position_127GB*1.02, f"124.3 GB/s", fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

# arrow_config = dict(facecolor='black', shrink=0.05, width=1, headwidth=4, headlength=5, linewidth=0.5)
# ax.annotate(f"", xy=(2, 35), xytext=(2, 70), fontsize = 8, arrowprops=arrow_config, ha='left', va='bottom')
# plt.text(1.2, 73, f"Only 2 Nodes in\nHW Baseline", fontsize = 9, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'left', va = 'bottom')

# Adding labels and title
ax.set_xlabel('Number of pages', fontsize = 9, weight = 'bold')
ax.set_ylabel('kIOPS', fontsize = 9, weight = 'bold')
ax.set_xticks(pages)
# ax.tick_params(axis='x', which='both', length=0, width=0)  # Adjust length and width as needed
# ax.set_xticklabels(cases)

legend = plt.legend()
ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.3), ncol=1)
legend.get_frame().set_linewidth(1)

#plt.xlim([1, 10])
#plt.ylim([0, 150])

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
save_plot('./section5', 'plot1_trace')
