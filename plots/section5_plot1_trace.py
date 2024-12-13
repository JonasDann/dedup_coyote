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
rounds = 35
cases = ['StreamDedup, mail', 'StreamDedup, web', 'StreamDedup, homes']
nodes = [1, 2, 4, 8, 10]

kiops_cpu = {"web": 1396.41, "mail": 1407.39, "homes": 2130.32}

kiops_mail = [3413, 3669.45 + 3667.37, 4377.39 + 4441.3 + 4354.56 + 4323.75, 4450.2 + 4414.42 + 4217.36 + 4337.57 + 4247.67 + 4194.16 + 4252.14 + 4284.45, 4208.28 + 4149.68 + 4176.94 + 4169.3 + 4560.81 + 4219.55 + 4237.48 + 4197.57 + 4360.1 + 4233.61]

kiops_web = [3349.72, 3648.38 + 3651.18, 3778.35 + 3786.12 + 3777.39 + 3811.37, 3867.56 + 3897.06 + 3938.7 + 3951.37 + 3959.62 + 3867.18 + 3839.66 + 3867.2, 3876.13 + 3946.48 + 3906.83 + 3882.02 + 3946.48 + 3865.52 + 3886.4 + 3875.11 + 3912.77 + 3963.7]

kiops_homes = [4334.99, 4119.66 + 3889.89, 4116.19 + 4364.13 + 3827.29 + 4040.2, 2686.24 + 2720.3 + 2694.22 + 2663.39 + 2660.06 + 2633.52 + 2747.3 + 2673.34, 2190.91 + 2218.86 + 2200.46 + 2554.33 + 2198.4 + 2273.95 + 2213.75 + 2200.86 + 2189.21 + 2234.1]

# Number of cases
n_cases = len(cases)

# Creating the bar plot
fig, ax = plt.subplots()

# default color: ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', '#8c564b', '#e377c2', '#7f7f7f', '#bcbd22', '#17becf']
line1, = plt.plot(nodes, kiops_mail, label=cases[0], marker='s', markersize = 10, zorder=3, linestyle='-', linewidth = 2, color = '#cc660b')
line2, = plt.plot([1], [kiops_cpu["mail"]], label="SW baseline, mail", marker='s', markersize = 10, zorder=3, linestyle='-', linewidth = 2, color = "#14517d")
line3, = plt.plot(nodes, kiops_web, label=cases[1], marker='d', markersize = 9, zorder=3, linestyle='-', linewidth = 2, color = '#ff7f0e')
line4, = plt.plot([1], [kiops_cpu["web"]], label="SW baseline, web", marker='d', markersize = 9, zorder=3, linestyle='-', linewidth = 2, color = "#1f77b4")
line5, = plt.plot(nodes, kiops_homes, label=cases[2], marker='x', markersize = 8, zorder=3, markeredgewidth=2, linewidth = 2, color = '#ffbf80')
line6, = plt.plot([1], [kiops_cpu["homes"]], label="SW baseline, homes", marker='x', markersize = 8, zorder=3, markeredgewidth=2, linewidth = 2, color = "#40abf5")
line1.set_clip_on(False)
line2.set_clip_on(False)
line3.set_clip_on(False)
line4.set_clip_on(False)
line5.set_clip_on(False)
line6.set_clip_on(False)

# line for 12.7
# position_127GB = 124.3
# plt.axhline(y = position_127GB, color = 'r', linestyle = 'dashed', linewidth = 1, zorder = 5)
# plt.text(5.5, position_127GB*1.02, f"124.3 GB/s", fontsize = 8, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'center', va = 'bottom')

arrow_config = dict(facecolor='black', shrink=0.05, width=1, headwidth=4, headlength=5, linewidth=0.5)
ax.annotate(f"", xy=(1.25, 1750), xytext=(2, 1750), fontsize = 8, arrowprops=arrow_config, ha='left', va='bottom')
plt.text(2.1, 1750, f"Only 1 node in SW baseline", fontsize = 9, rotation=0, rotation_mode='anchor', weight = 'bold', ha = 'left', va = 'center')

# Adding labels and title
ax.set_xlabel('Number of nodes', fontsize = 9, weight = 'bold')
ax.set_ylabel('kIOPS', fontsize = 9, weight = 'bold')
ax.set_xticks(list(range(11)))
# ax.tick_params(axis='x', which='both', length=0, width=0)  # Adjust length and width as needed
# ax.set_xticklabels(cases)

legend = plt.legend()
ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.35), ncol=3)
legend.get_frame().set_linewidth(1)

plt.xlim([1, 10])
#plt.ylim([0, 150])
plt.yscale("log")

# Adjusting the layout
plt.subplots_adjust(left=0.105, right=0.905, top=0.79, bottom=0.17)
# Alternatively
# plt.tight_layout()

# Function to save the plot in a given directory in both PNG and PDF formats
def save_plot(directory, filename):
    # Create the directory if it doesn't exist
    os.makedirs(directory, exist_ok=True)
    plt.savefig(f"{directory}/{filename}.png")
    plt.savefig(f"{directory}/{filename}.pdf")

# Example usage (replace 'your_directory_path' with the actual path)
save_plot('./plots/section5', 'plot1_trace')
