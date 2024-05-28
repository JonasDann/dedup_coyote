import os
import subprocess
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
import re
import time

def run_command_on_server(hostname, command, output_dir, print_en = False):
    # Construct the SSH command
    ssh_command = f"ssh {hostname} '{command}'"

    # # Execute the command and capture the output
    # result = subprocess.run(ssh_command, shell=True, capture_output=True)

    # # Convert bytes to string for printing and writing
    # output_str = result.stdout.decode('utf-8')

    # if(print_en):
    #     print(f"execution: {command}")
    #     print(output_str)

    # # Write output to a file in the specified directory
    # with open(f"{output_dir}/{hostname}.txt", "w") as file:
    #     file.write(f"execution: {command}\n")
    #     file.write(output_str)

    # return output_str
    process = subprocess.Popen(ssh_command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    # Open the output file
    with open(f"{output_dir}/{hostname}.txt", "w") as file:
        if print_en:
            print(f"execution: {command}")
        file.write(f"execution: {command}\n")
        # Read from the process output line by line
        for line in iter(process.stdout.readline, ''):
            # Print to console if this is the first host
            if print_en:
                print(line, end='')

            # Write to file
            file.write(line)

    # Wait for the process to finish and get the exit code
    process.wait()

    return process.returncode

def analyze_results(hostnames, output_dir):
    command_results = {}
    time_results = {}

    for hostname in hostnames:
        output_file = f"{output_dir}/{hostname}.txt"
        if not os.path.exists(output_file):
            raise FileNotFoundError(f"Output file for {hostname} not found: {output_file}")

        with open(output_file, "r") as file:
            content = file.read()

        # Split the content by "execution:" to separate different command outputs
        exec_blocks = content.split("execution:")
        if len(exec_blocks) < 0:
            raise ValueError(f"No execution data found in {output_file}")
        for block in exec_blocks[1:]:  # Skip the first split as it's empty
            # Extract the command
            command = block.split("\n")[0].strip()

            # Find all "all page passed" instances
            passed = all("all page passed?: True" in line for line in block.split("\n") if "all page passed?:" in line)

            # Find the average time used
            time_match = re.search("benchmarking done, avg time used: ([\d\.e\+]+) ns", block)
            time = float(time_match.group(1)) if time_match else None

            # Store results
            command_results.setdefault(command, []).append(passed)
            time_results.setdefault(command, []).append(time)

    # Print results
    for command, passed_list in command_results.items():
        all_passed = all(passed_list)
        avg_time = sum(time_results[command]) / len(time_results[command])
        print(f"############################################")
        print(f"{command}")
        print(f"- All pages passed: {all_passed} - Average time across hosts: {avg_time} ns")
        for hostname, time in zip(hostnames, time_results[command]):
            print(f"{hostname}: {time} ns")
        print(f"############################################")

def main():
    # List of hostnames and commands
    hostnames = ["alveo-u55c-01",
                 "alveo-u55c-02",
                 "alveo-u55c-03",
                 "alveo-u55c-04",
                 "alveo-u55c-05",
                 "alveo-u55c-06",
                 "alveo-u55c-07",
                 "alveo-u55c-08",
                 "alveo-u55c-09",
                 "alveo-u55c-10",] 
    host_ips = ["alveo-u55c-01,10.253.74.66,10.253.74.68",
                "alveo-u55c-02,10.253.74.70,10.253.74.72",
                "alveo-u55c-03,10.253.74.74,10.253.74.76",
                "alveo-u55c-04,10.253.74.78,10.253.74.80",
                "alveo-u55c-05,10.253.74.82,10.253.74.84",
                "alveo-u55c-06,10.253.74.86,10.253.74.88",
                "alveo-u55c-07,10.253.74.90,10.253.74.92",
                "alveo-u55c-08,10.253.74.94,10.253.74.96",
                "alveo-u55c-09,10.253.74.98,10.253.74.100",
                "alveo-u55c-10,10.253.74.102,10.253.74.104",]
    # Define the parameter ranges
    # num_pages = ["16384"]  # Add your values
    # param1 = [0]  # Add your values
    # fullnesses = [0]  # Add your values
    # commands = ['ls', 'whoami']  # Replace with your commands
    command_lst = [
    "/home/jiayli/projects/coyote-rdma/sw/build/main -n 16384 -d 1 -f 0.9921875 -s 1 -v 0",
    "/home/jiayli/projects/coyote-rdma/sw/build/main -n 16384 -d 0 -f 0.9296875 -s 1 -v 0",
    # "/home/jiayli/projects/coyote-rdma/sw/build/main -n 16384 -g 1 -f 0.9921875 -s 1 -v 0",
    # "/home/jiayli/projects/coyote-rdma/sw/build/main -n 16384 -g 0 -f 0.9921875 -s 1 -v 0",
    # "/home/jiayli/projects/coyote-rdma/sw/build/main -n 16384 -f 0.9921875 -s 1 -v 0",

    "/home/jiayli/projects/coyote-rdma/sw/build/main -n 16 -d 1 -f 0.9921875 -s 1 -v 0",
    "/home/jiayli/projects/coyote-rdma/sw/build/main -n 16 -d 0 -f 0.9921875 -s 1 -v 0",
    # "/home/jiayli/projects/coyote-rdma/sw/build/main -n 16 -g 1 -f 0.9921875 -s 1 -v 0",
    # "/home/jiayli/projects/coyote-rdma/sw/build/main -n 16 -g 0 -f 0.9921875 -s 1 -v 0",
    # "/home/jiayli/projects/coyote-rdma/sw/build/main -n 16 -f 0.9921875 -s 1 -v 0",
    ]

    for command in command_lst:
        # for node_used in range(1, len(hostnames) + 1):
        # for node_used in range(6, 11):
        for node_used in [10]:
            active_hosts = hostnames[0:node_used]
            # routing table gen
            with open("./server_ip.csv", 'w') as file:
                for entry in host_ips[0:node_used]:
                    file.write(f"{entry}\n")
            # config update
            # subprocess.run("python3 routing_table_gen.py --hostlist " + ' '.join(active_hosts) + " --mode equal_divide", shell=True, capture_output=True)
            # subprocess.run("python3 routing_table_gen.py --hostlist " + ' '.join(active_hosts) + " --mode all2all", shell=True, capture_output=True)
            subprocess.run("python3 routing_table_gen.py --hostlist " + ' '.join(active_hosts) + " --mode chord", shell=True, capture_output=True)
            subprocess.run("python3 rdma_connection_gen.py", shell=True, capture_output=True)
            # Get current timestamp and create a directory
            timestamp = datetime.now().strftime("%Y_%m%d_%H%M_%S")
            output_dir = f"./experiment_tmp/node_sweep/{timestamp}"
            os.makedirs(output_dir, exist_ok=True)
            print(f"start execution: {node_used=}, {output_dir=}")
            # Using ThreadPoolExecutor to run commands concurrently
            with ThreadPoolExecutor(max_workers=node_used) as executor:
                futures = [executor.submit(run_command_on_server, hostname, command, output_dir, hostname == active_hosts[0]) for hostname in active_hosts]

            # Waiting for all futures to complete
            for future in futures:
                future.result()
            print(f"end execution: {node_used=}, {output_dir=}, analyzing results...")
            analyze_results(active_hosts, output_dir)
            # analyze_results(active_hosts, "./experiment_tmp/node_sweep/2023_1228_1627_22")

            for count_down in range(70, 0, -10):
                print(f"waiting, {count_down} seconds remaining ...")
                time.sleep(10)

if __name__ == "__main__":
    main()
