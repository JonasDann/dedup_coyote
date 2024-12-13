import glob
import os
import math
import sys

name = sys.argv[1]
partition_count = int(sys.argv[2])
folder = "/pub/scratch/jodann/dedup_data/" + name
output_folder = folder + "/partitioned_" + str(partition_count)
if not os.path.exists(output_folder):
  print("Create folder " + output_folder)
  os.makedirs(output_folder)
else:
  print("Partitions already exist")
  sys.exit()
page_count = 0
ne_read_count = 0
max_pages_per_partition = 260096
curr_partition = 0
max_partitions = 10
page_map = dict()
total_line_count = 0
values = ["0"]

counted_filename = os.path.join(folder, name + ".counted.")
ners_filename = os.path.join(folder, name + ".ners.")
if not os.path.exists(counted_filename + "0") or not os.path.exists(ners_filename + "0"):
  nf = open(ners_filename + "0", "w")
  of = open(counted_filename + "0", "w")

  print("Starting to write to " + counted_filename + "0 and " + ners_filename + "0 trying to obtain " + str(max_partitions) + " partitions")
  for filename in sorted(glob.glob(os.path.join(folder, "*.blkparse"))):
    print("Reading file " + filename)
    with open(filename, "r") as f:
      prev_line = ""
      line_count = 0
      for line in f.readlines():
        if line != prev_line: # Deduplicate lines
          values = line.split(" ")
          if len(values) == 9:
            if values[8] in page_map: # If page already exists
              of.write(values[3] + " " + values[5] + " " + str(page_map[values[8]]) + "\n")
            else: # If pages does not exist
              of.write(values[3] + " " + values[5] + " " + str(page_count) + "\n")
              if values[5] == "R": # If page is read, thus, being a non-existent read
                nf.write(str(page_count) + "\n")
                ne_read_count += 1
              page_map[values[8]] = page_count
              page_count += 1
            line_count += 1
            if page_count >= (curr_partition + 1) * max_pages_per_partition: # If current partition full
              print("Reached max number of pages in partition")
              curr_partition += 1
              if curr_partition >= max_partitions:
                print("Reached max number of partitions")
                break
              print("Starting to write to " + counted_filename + str(curr_partition) + " and " + ners_filename + str(curr_partition))
              nf.close()
              of.close()
              nf = open(ners_filename + str(curr_partition), "w")
              of = open(counted_filename + str(curr_partition), "w")
          prev_line = line
      print("File has " + str(line_count) + " lines")
      total_line_count += line_count
      if curr_partition >= max_partitions:
        break

  nf.close()
  of.close()

  print("Last timestamp: " + values[0])
  print("Total line count: " + str(total_line_count))
  print("Unique page count: " + str(page_count))
  print("Non-existent read count: " + str(ne_read_count))
else:
  print("Skip first step because " + name + ".counted and non_existent_reads.txt already exist")

###
# Stripe trace over partition_count files
###
print("\nStarting to partition trace")
partition = 0
curr_line = 0
ofs = []
for i in range(partition_count):
  ofs.append(open(os.path.join(output_folder, name + ".trace." + str(i)), "w"))
for i in range(1): # TODO Change these back to partition_count for larger benchmarks
  print("Reading file " + counted_filename + str(i))
  with open(counted_filename + str(i), "r") as f:
    for line in f:
      ofs[curr_line % partition_count].write(line)
      curr_line += 1
for i in range(partition_count):
  ofs[i].close()

print("Starting to partition non-existent reads")
curr_line = 0
nerfs = []
for i in range(partition_count):
  nerfs.append(open(os.path.join(output_folder, name + ".ners." + str(i)), "w"))
for i in range(1): # TODO Change these back to partition_count for larger benchmarks
  print("Reading file " + ners_filename + str(i))
  with open(ners_filename + str(i), "r") as f:
    for line in f:
      nerfs[curr_line % partition_count].write(line)
      curr_line += 1
for i in range(partition_count):
  nerfs[i].close()
