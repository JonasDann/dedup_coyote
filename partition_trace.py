import glob
import os
import math
import sys

name = "mail"
folder = "/pub/scratch/jodann/dedup_data/" + name
page_count = 0
ne_read_count = 0
partition_count = 10
max_pages = partition_count * 260096
page_map = dict()
total_line_count = 0
values = ["0"]
counted_filename = os.path.join(folder, name + ".counted")
with open(os.path.join(folder, "non_existent_reads.txt"), "w") as nerf:
  with open(counted_filename, "w") as of:
    print("Writing to " + counted_filename)
    for filename in sorted(glob.glob(os.path.join(folder, "*.blkparse"))):
      print("Reading file " + filename)
      with open(filename, "r") as f:
        prev_line = ""
        line_count = 0
        for line in f.readlines():
          if line != prev_line:
            values = line.split(" ")
            if len(values) == 9:
              if values[8] in page_map:
                of.write(values[3] + " " + values[5] + " " + str(page_map[values[8]]) + "\n")
              else:
                of.write(values[3] + " " + values[5] + " " + str(page_count) + "\n")
                if values[5] == "R":
                  nerf.write(str(page_count) + "\n")
                  ne_read_count += 1
                page_map[values[8]] = page_count
                page_count += 1
                if page_count >= max_pages:
                  break
              line_count += 1
            prev_line = line
        print("File has " + str(line_count) + " lines")
        total_line_count += line_count
      if page_count >= max_pages:
        break

print("Last timestamp: " + values[0])
print("Total line count: " + str(total_line_count))
print("Unique page count: " + str(page_count))
print("Non-existent read count: " + str(ne_read_count))

print("\nStarting partitioning")
partition_size = math.ceil(total_line_count / partition_count)
partition = 0
curr_line = 0
ofs = []
for i in range(partition_count):
  ofs.append(open(os.path.join(folder, name + ".trace." + str(i)), "w"))
print("Reading file " + counted_filename)
with open(counted_filename, "r") as f:
  for line in f:
    ofs[curr_line % partition_count].write(line)
    curr_line += 1
for i in range(partition_count):
  ofs[i].close()

print("Partitioning non-existent reads")
ne_partition_size = math.ceil(ne_read_count / partition_count)
curr_line = 0
nerfs = []
for i in range(partition_count):
  nerfs.append(open(os.path.join(folder, name + ".nereads." + str(i)), "w"))
with open(os.path.join(folder, "non_existent_reads.txt"), "r") as f:
  for line in f:
    nerfs[curr_line % partition_count].write(line)
    curr_line += 1
for i in range(partition_count):
  nerfs[i].close()
