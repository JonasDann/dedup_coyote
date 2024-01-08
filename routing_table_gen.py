#!/usr/bin/python3

import argparse
import os
from datetime import datetime
import math

def parse_arguments():
  parser = argparse.ArgumentParser(description='Calculate routing table based on mode.')
  parser.add_argument('--mode', default='equal_divide', type=str, help='Mode of operation (e.g., equal_divide, hop_test)')
  parser.add_argument('--hop-count', type=int, help='Hop count for hop_test mode', default=0)
  parser.add_argument('--hostlist', nargs='+', help='List of host names')
  return parser.parse_args()

# Usage
# python3 routing_table_gen.py --hostlist hostname1 hostname2 hostname3 hostname4 hostname5 hostname6 hostname7 hostname8 hostname9 --mode hop_test --hop-count 0
# python3 routing_table_gen.py --hostlist hostname1 hostname2 hostname3 hostname4 hostname5 hostname6 hostname7 hostname8 hostname9 --mode equal_divide

###############################
##
## Chord Rounding
##
###############################
def calculate_equal_division(node_count, bit_width, start = 0):
  circle_size = 1 << bit_width
  step = circle_size / node_count  # Equal division step
  assert step >= 1
  return [(start + math.floor(i * step))%circle_size for i in range(node_count)]

def write_routing_table_to_file(routing_table, directory, file_name):
  with open(os.path.join(directory, file_name), 'w') as file:
    for entry in routing_table:
      file.write(f"{','.join(map(str, entry))}\n")

def generate_local_routing_table_dense(full_routing_table, directory, node_count):
  half_node_count = node_count / 2
  max_shift = math.floor(math.log2(half_node_count)) # max possible 2^n in half_node_count
  for i, (hostname, _, _, _) in enumerate(full_routing_table):
    file_name = f"routing_table_{i}_{hostname}.csv"
    with open(os.path.join(directory, file_name), 'w') as file:
      written_indices = set()
      file.write(f"{','.join(map(str, full_routing_table[i]))}\n")
      written_indices.add(i)
      for shift in range(0, max_shift + 1):
        j1 = (i + (1 << shift)) % node_count
        if j1 not in written_indices:
          file.write(f"{','.join(map(str, full_routing_table[j1]))}\n")
          written_indices.add(j1)
        j2 = (i - (1 << shift)) % node_count
        if j2 != j1:
          if j2 not in written_indices:
            file.write(f"{','.join(map(str, full_routing_table[j2]))}\n")
            written_indices.add(j2)
    print(f"{hostname} local routing table saved to {os.path.join(directory, file_name)}")
  
def generate_local_routing_table_all2all(full_routing_table, directory, node_count):
  for i, (hostname, _, _, _) in enumerate(full_routing_table):
    file_name = f"routing_table_{i}_{hostname}.csv"
    with open(os.path.join(directory, file_name), 'w') as file:
      written_indices = set()
      file.write(f"{','.join(map(str, full_routing_table[i]))}\n")
      written_indices.add(i)
      for shift in range(0, node_count):
        j = (i + shift) % node_count
        if j not in written_indices:
          file.write(f"{','.join(map(str, full_routing_table[j]))}\n")
          written_indices.add(j)
    print(f"{hostname} local routing table saved to {os.path.join(directory, file_name)}")

def generate_local_routing_table_chord(full_routing_table, directory, node_count):
  max_shift = math.floor(math.log2(node_count)) # max possible 2^n in node_count
  for i, (hostname, _, _, _) in enumerate(full_routing_table):
    file_name = f"routing_table_{i}_{hostname}.csv"
    with open(os.path.join(directory, file_name), 'w') as file:
      written_indices = set()
      file.write(f"{','.join(map(str, full_routing_table[i]))}\n")
      written_indices.add(i)
      for shift in range(0, max_shift + 1):
        j1 = (i + (1 << shift)) % node_count
        if j1 not in written_indices:
          file.write(f"{','.join(map(str, full_routing_table[j1]))}\n")
          written_indices.add(j1)
    print(f"{hostname} local routing table saved to {os.path.join(directory, file_name)}")
       

###############################
##
## Hop Test
##
###############################
def generate_hop_test_routing_table(hostlist, node_idx_values, directory, hop_count, routing_decision_bits):
  node_count = len(hostlist)
  half_node_count = node_count / 2
  max_shift = math.floor(math.log2(half_node_count)) # max possible 2^n in half_node_count
  for i, hostname in enumerate(hostlist):
    file_name = f"routing_table_{i}_{hostname}.csv"
    with open(os.path.join(directory, file_name), 'w') as file:
      written_indices = set()
      if i < hop_count:
        # These nodes think the next node holds all hashes
        file.write(f"{hostname},{node_idx_values[i]},0,0\n")
        written_indices.add(i)
        next_node_idx = (i + 1) % node_count
        file.write(f"{hostlist[next_node_idx]},{node_idx_values[next_node_idx]},0,{1 << routing_decision_bits}\n")
        written_indices.add(next_node_idx)
        prev_node_idx = (i - 1) % node_count
        if prev_node_idx != next_node_idx:
          file.write(f"{hostlist[prev_node_idx]},{node_idx_values[prev_node_idx]},0,0\n")
          written_indices.add(prev_node_idx)
      else:
        # These nodes think they hold all hashes
        file.write(f"{hostname},{node_idx_values[i]},0,{1 << routing_decision_bits}\n")
        written_indices.add(i)
        next_node_idx = (i + 1) % node_count
        file.write(f"{hostlist[next_node_idx]},{node_idx_values[next_node_idx]},0,0\n")
        written_indices.add(next_node_idx)
        prev_node_idx = (i - 1) % node_count
        if prev_node_idx != next_node_idx:
          file.write(f"{hostlist[prev_node_idx]},{node_idx_values[prev_node_idx]},0,0\n")
          written_indices.add(prev_node_idx)
      for shift in range(1, max_shift + 1):
        j1 = (i + (1 << shift)) % node_count
        if j1 not in written_indices:
          file.write(f"{hostlist[j1]},{node_idx_values[j1]},0,0\n")
          written_indices.add(j1)
        j2 = (i - (1 << shift)) % node_count
        if j2 != j1:
          if j2 not in written_indices:
            file.write(f"{hostlist[j2]},{node_idx_values[j2]},0,0\n")
            written_indices.add(j2)
    print(f"{hostname} local routing table saved to {os.path.join(directory, file_name)}")

def main():
  # Generate timestamped directory name
  time_stamp = datetime.now().strftime("%Y_%m%d_%H%M_%S")
  save_directory = os.path.join("./routing_table", time_stamp)
  os.makedirs(save_directory, exist_ok=True)

  args = parse_arguments()
  mode = args.mode
  hop_count = args.hop_count
  hostlist = args.hostlist

  # Constants - you can adjust these values as needed
  node_count = len(hostlist)
  node_idx_width = 4
  routing_channel_count = 8
  routing_decision_bits = 16

  if mode in ['all2all', 'equal_divide', 'chord']:
    print(f"Generating Routing Table for {node_count=} with equal_divide mode")
    # Divide the ring equally for nodeIdx
    node_idx_values = calculate_equal_division(node_count, node_idx_width)
    # Divide the ring equally for hash start
    hash_start_values = calculate_equal_division(node_count, routing_decision_bits)
    hash_len_values = []
    for i in range(node_count):
      next_idx = (i + 1) % node_count
      diff = hash_start_values[next_idx] - hash_start_values[i]
      hash_len_values.append(diff % (1 << routing_decision_bits))

    # routing_table = zip(hostlist, node_idx_values, hash_start_values, hash_len_values)
    full_table_file_name="routing_table_full.csv"
    write_routing_table_to_file(zip(hostlist, node_idx_values, hash_start_values, hash_len_values), save_directory, full_table_file_name)
    print(f"Full routing table saved to {os.path.join(save_directory, full_table_file_name)}")
    # import pdb;pdb.set_trace()
    if mode == 'equal_divide':
      generate_local_routing_table_dense(list(zip(hostlist, node_idx_values, hash_start_values, hash_len_values)), save_directory, len(hostlist))
    elif mode == 'all2all':
      generate_local_routing_table_all2all(list(zip(hostlist, node_idx_values, hash_start_values, hash_len_values)), save_directory, len(hostlist))
    else:
      generate_local_routing_table_chord(list(zip(hostlist, node_idx_values, hash_start_values, hash_len_values)), save_directory, len(hostlist))
  elif mode == 'hop_test':
    if (hop_count + 1) > len(hostlist):
      raise ValueError('Host count is not enough to support the target hop count')
    print(f"Generating Routing Table for {node_count=} with hop_test mode, {hop_count=}")
    # Divide the ring equally for nodeIdx
    node_idx_values = calculate_equal_division(node_count, node_idx_width)
    assert hop_count <= node_count
    generate_hop_test_routing_table(hostlist, node_idx_values, save_directory, hop_count, routing_decision_bits)
  else:
    raise NotImplementedError

if __name__ == "__main__":
  main()
