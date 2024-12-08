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

def custom_autopct(pct, allvals):
    absolute = int(pct/100.*sum(allvals))
    threshold = 20  # Set threshold for placing label inside or outside
    if pct > threshold:
        return "{:.1f}%".format(pct)
    else:
        return ""

clb_dedup = 68640
clb_fingerprint = 63166
clb_hashtable = 3508
clb_routingtable = 1154
clb_others = clb_dedup - clb_fingerprint - clb_hashtable- clb_routingtable

bram_dedup = 275.5
bram_fingerprint = 192
bram_hashtable = 2
bram_routingtable = 0
bram_others = bram_dedup - bram_fingerprint - bram_hashtable- bram_routingtable

# Data to plot
labels = 'Fingerprint', 'Hash Table', 'Routing Table', 'Others'
clb_sizes = [clb_fingerprint, clb_hashtable, clb_routingtable, clb_others]
bram_sizes = [bram_fingerprint, bram_others]
colors = ['lightskyblue', 'gold', 'purple', 'lightcoral']

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(4, 2))
plt.subplots_adjust(wspace=0.05)
# Water usage pie chart

# ax1.pie(clb_sizes, colors=colors,
#         autopct='%1.1f%%', shadow=False, startangle = 90, counterclock=True)
wedges1, texts1, autotexts1 = ax1.pie(clb_sizes, colors=colors, autopct=lambda pct: custom_autopct(pct, clb_sizes), 
                                      shadow=False, startangle = 90, counterclock=True)


# ax1.set_title('Water Usage')
# ax1.set_xlabel('CLB Utilization')
ax1.text(0, -1.2, 'CLB Usage', ha='center', va = 'center')

# Gas usage pie chart
wedges2, texts2, autotexts2  = ax2.pie(bram_sizes, colors=[colors[0], colors[-1]],
    autopct='%1.1f%%', shadow=False, startangle = 90, counterclock=True)
# ax2.set_title('Gas Usage')
# ax2.set_xlabel('BRAM Utilization')
ax2.text(0, -1.2, 'BRAM Usage', ha='center', va = 'center')
# plt.pie(sizes, explode=explode, labels=labels, colors=colors,
#     autopct='%1.1f%%', shadow=False, startangle = 90,counterclock=False, pctdistance=1.1, labeldistance=1.2)

ax1.axis('equal')  # Equal aspect ratio ensures that pie is drawn as a circle.
ax2.axis('equal')  # Equal aspect ratio ensures that pie is drawn as a circle.
# Create the legend
fig.legend(labels, loc='upper center', 
           bbox_to_anchor=(0.5, 1), ncol=4, handlelength=0.7, handletextpad=0.5, markerfirst=True,
           columnspacing = 0.8)
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
save_plot('./plot_pie2', 'dedup_util')