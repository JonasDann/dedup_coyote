import os
import sys
import re
from datetime import datetime
import itertools

def read_routing_table(node_idx, hostname, file_content):
  lines = file_content.strip().split('\n')
  source_host, source_node_id, *_ = lines[0].split(',')

  # Verify the first line is the host itself
  if not source_host == hostname:
    raise ValueError(f"First line of file does not correspond to the host itself: {source_host=}, {hostname=}")

  connections = []

  # Process each line to identify outgoing connections
  for line in lines[1:]:
    target_host, target_node_id, _, _ = line.split(',')
    connections.append((source_host, source_node_id, target_host, target_node_id))

  return connections


def main():
  # Check if the correct number of arguments are passed
  # if len(sys.argv) != 3:
  #   print("Usage: python script.py <source_directory> <destination_directory>")
  #   print("python3 rdma_connection_gen.py ./routing_table ./rdma_connection")
  #   sys.exit(1)

  # Extract directories from arguments
  source_directory = "./routing_table"
  destination_directory = "./rdma_connection"
  ip_info_path="./server_ip.csv"

  # Check if source directory exists
  if not os.path.exists(source_directory):
    print(f"Source directory '{source_directory}' does not exist.")
    sys.exit(1)

  # Create destination directory if it does not exist
  if not os.path.exists(destination_directory):
    os.makedirs(destination_directory)
    print(f"Destination directory '{destination_directory}' was created.")

  # Generate timestamped directory name
  time_stamp = datetime.now().strftime("%Y_%m%d_%H%M_%S")
  save_directory = os.path.join(destination_directory, time_stamp)
  os.makedirs(save_directory, exist_ok=True)

  # Find the latest timestamped subdirectory
  latest_subdir = None
  latest_time = None
  for subdir in os.listdir(source_directory):
    try:
      subdir_time = datetime.strptime(subdir, "%Y_%m%d_%H%M_%S")
      if latest_time is None or subdir_time > latest_time:
        latest_time = subdir_time
        latest_subdir = subdir
    except ValueError:
      # Skip directories that don't match the expected time format
      continue

  if latest_subdir is None:
    print("No valid timestamped subdirectories found.")
    sys.exit(1)

  latest_subdir_path = os.path.join(source_directory, latest_subdir)
  print(f"Processing files in the latest subdirectory: {latest_subdir_path}")

  # Regular expression to match and extract nodeIdx from file names
  # \w matches any alphanumeric character, \- for hyphens, and \. for dots
  pattern = re.compile(r'routing_table_(\d+)_([\w\-\.]+)\.csv')

  # List to store files and their node indices
  files_to_process = []

  # Loop over files in the latest subdirectory and filter based on pattern
  for file in os.listdir(latest_subdir_path):
    match = pattern.match(file)
    if match:
      node_idx = int(match.group(1))
      hostname = match.group(2)
      files_to_process.append((node_idx, hostname, file))

  # Sort files based on nodeIdx
  files_to_process.sort(key=lambda x: x[0])

  # Convert files_to_process to a dictionary for faster lookup
  hostname_info = {hostname: [node_idx] for node_idx, hostname, _ in files_to_process}
  all_connections = [] # (source_host, source_node_id, target_host, target_node_id)

  # get all ips
  with open(ip_info_path, 'r') as f:
    ip_info = f.read()
    lines = ip_info.strip().split('\n')
    # Process each line to identify ips
    for line in lines:
      hostname, cpu_ip, fpga_ip = line.split(',')
      if hostname in hostname_info:
        assert len(hostname_info[hostname]) == 1
        hostname_info[hostname].extend([cpu_ip, fpga_ip])

  for key in hostname_info.keys():
    assert len(hostname_info[key]) == 3

  # Process files in sorted order
  for node_idx, hostname, file in files_to_process:
    file_path = os.path.join(latest_subdir_path, file)
    print(f"Processing file: {file_path} (Node Index: {node_idx}, Hostname: {hostname})")
    with open(file_path, 'r') as f:
      file_content = f.read()

      try:
        connections = read_routing_table(node_idx, hostname, file_content)
        all_connections.extend(connections) # concat results
      except ValueError as e:
        print(f"Error processing file '{file_path}': {e}")
        continue
  
  outgoing_connections = {hostname: [] for _, hostname, _ in files_to_process}
  incomming_connections = {hostname: [] for _, hostname, _ in files_to_process}

  for index, (hostname0, node_id0, hostname1, node_id1) in enumerate(all_connections):
    node_idx0 = hostname_info[hostname0][0]
    node_idx1 = hostname_info[hostname1][0]
    # Append the new tuple format to the list
    outgoing_connections[hostname0].append([node_idx1, hostname1, node_id1, index] + hostname_info[hostname1][1:])
    incomming_connections[hostname1].append([node_idx0, hostname0, node_id0, index] + hostname_info[hostname0][1:])

  # Sort the tuples for each hostname and write them to CSV files
  for hostname, connections in outgoing_connections.items():
    # Sort the connections by node_idx0
    connections.sort(key=lambda x: x[0])
    # Define the CSV file path
    node_idx = hostname_info[hostname][0]
    file_path = os.path.join(save_directory, f"rdma_m2s_{node_idx}_{hostname}.csv")
    # Write the connections to the CSV file
    with open(file_path, 'w', newline='') as file:
      for connection in connections:
        file.write(f"{','.join(map(str, connection))}\n")
  
  for hostname, connections in incomming_connections.items():
    # Sort the connections by node_idx0
    connections.sort(key=lambda x: x[0])
    # Define the CSV file path
    node_idx = hostname_info[hostname][0]
    file_path = os.path.join(save_directory, f"rdma_s2m_{node_idx}_{hostname}.csv")
    # Write the connections to the CSV file
    with open(file_path, 'w', newline='') as file:
      for connection in connections:
        file.write(f"{','.join(map(str, connection))}\n")
  
  # Get updated routing table with connection id
  for node_idx, hostname, file in files_to_process:
    input_file_path = os.path.join(latest_subdir_path, file)
    output_file_path = os.path.join(save_directory, f"local_config_{node_idx}_{hostname}.csv")
    with open(input_file_path, mode='r') as infile, \
         open(output_file_path, mode='w') as outfile:
      input_file_content = infile.read()
      lines = input_file_content.strip().split('\n')
      for line in lines:
        entry_hostname, node_id, hash_start, hash_end = line.split(',')
        if (entry_hostname == hostname):
          outfile.write(f"{','.join(map(str, [entry_hostname, node_id, hash_start, hash_end, 0, *hostname_info[entry_hostname]]))}\n")
        else:
          # find connection id
          for connection_info in outgoing_connections[hostname]:
            if (connection_info[1] == entry_hostname):
              outfile.write(f"{','.join(map(str, [entry_hostname, node_id, hash_start, hash_end, connection_info[3], *hostname_info[entry_hostname]]))}\n")


if __name__ == "__main__":
    main()
