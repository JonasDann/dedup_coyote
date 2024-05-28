import matplotlib.pyplot as plt
import os

plt.rcParams.update({
      'font.size'        : 9, 
      'font.weight'      : 'bold', 
      'figure.facecolor' : 'w',
      'figure.dpi'       : 500,
      'figure.figsize'   : (4, 3),
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
      'axes.grid'        : True,
      'grid.linewidth'   : '1',
      'legend.fancybox'  : False,
      'legend.framealpha': 0,
      'legend.edgecolor' : 'black',
      # case dependent
      'axes.autolimit_mode': 'round_numbers', # 'data' or 'round_numbers'
      'lines.linewidth'  : 1,
      'lines.markersize' : 6,
    })

clb_total = 162960
clb_coyote = 113206
clb_dedup = 68640
clb_coyote_shell = clb_coyote - clb_dedup
clb_not_used = clb_total - clb_coyote

bram_total = 2016
bram_coyote = 508
bram_dedup = 275.5
bram_coyote_shell = bram_coyote - bram_dedup
bram_not_used = bram_total - bram_coyote

# Data to plot
labels = 'Dedup Core', 'Coyote Shell', 'Not Used'
clb_sizes = [clb_dedup, clb_coyote_shell, clb_not_used]
bram_sizes = [bram_dedup, bram_coyote_shell, bram_not_used]
colors = ['gold', 'lightskyblue', 'lightcoral', 'yellowgreen', ]
explode = (0, 0, 0)  # explode 1st slice

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(4, 2))
plt.subplots_adjust(wspace=0.05)
# Water usage pie chart
ax1.pie(clb_sizes, explode=explode, colors=colors,
    autopct='%1.1f%%', shadow=False, startangle = 90, counterclock=True)
# ax1.set_title('Water Usage')
# ax1.set_xlabel('CLB Utilization')
ax1.text(0, -1.2, 'CLB Usage', ha='center', va = 'center')

# Gas usage pie chart
ax2.pie(bram_sizes, explode=explode, colors=colors,
    autopct='%1.1f%%', shadow=False, startangle = 90, counterclock=True)
# ax2.set_title('Gas Usage')
# ax2.set_xlabel('BRAM Utilization')
ax2.text(0, -1.2, 'BRAM Usage', ha='center', va = 'center')
# plt.pie(sizes, explode=explode, labels=labels, colors=colors,
#     autopct='%1.1f%%', shadow=False, startangle = 90,counterclock=False, pctdistance=1.1, labeldistance=1.2)

ax1.axis('equal')  # Equal aspect ratio ensures that pie is drawn as a circle.
ax2.axis('equal')  # Equal aspect ratio ensures that pie is drawn as a circle.
# Create the legend
fig.legend(labels, loc='upper center', bbox_to_anchor=(0.5, 1), ncol=3, handlelength=0.7, handletextpad=0.5, markerfirst=True)
# legend = plt.legend()
# ax.legend(loc='upper center', bbox_to_anchor=(0.5, 1.25), ncol=2, handlelength=0.7, handletextpad=0.5, markerfirst=True)
# legend.get_frame().set_linewidth(1)
# plt.title('Water Utilization by Region')
plt.subplots_adjust(left=0.1, right=0.9, top=0.85, bottom=0.1)

# Function to save the plot in a given directory in both PNG and PDF formats
def save_plot(directory, filename):
    # Create the directory if it doesn't exist
    os.makedirs(directory, exist_ok=True)
    plt.savefig(f"{directory}/{filename}.png")
    plt.savefig(f"{directory}/{filename}.pdf")

# Example usage (replace 'your_directory_path' with the actual path)
save_plot('./plot_pie1', 'all_util')