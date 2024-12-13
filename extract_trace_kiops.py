import glob

values = [0 for _ in list(range(0, 100))]
files = glob.glob("./experiment_tmp/trace/homes_trace/*")
for filename in files:
  i = 0
  with open(filename, "r") as f:
    for line in f.readlines():
      if line.startswith("kIOPS"):
       values[i] += float(line.split()[1])
       i += 1
print([None if v == 0 else v for v in values[0:-2]])
