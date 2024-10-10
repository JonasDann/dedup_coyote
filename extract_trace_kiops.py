import glob

values = list(range(0, 40))
files = glob.glob("./experiment_tmp/trace/web_trace/*")
for filename in files:
  i = 0
  with open(filename, "r") as f:
    for line in f.readlines():
      if line.startswith("kIOPS"):
       values[i] += float(line.split()[1])
       i += 1
print([v / len(files) for v in values])
