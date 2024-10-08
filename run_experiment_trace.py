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
    print(time_results)
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
    hostnames = ["alveo-u55c-01",
                 "alveo-u55c-02",
                 "alveo-u55c-03",
                 "alveo-u55c-04",
                 "alveo-u55c-05",
                 "alveo-u55c-06",
                 "alveo-u55c-07",
                 "alveo-u55c-08",
                 "alveo-u55c-09",
                 "alveo-u55c-10"] 
    command = "cd " + str(os.getcwd()) + " && " + str(os.getcwd()) + "/sw/build/main -n 16384 -f 0.9921875 -s 1 -v 1 -t "

    for trace in ["web"]:
        for node_used in [10]:
            # Get current timestamp and create a directory
            timestamp = datetime.now().strftime("%Y_%m%d_%H%M_%S")
            output_dir = f"./experiment_tmp/trace/{timestamp}"
            os.makedirs(output_dir, exist_ok=True)
            print(f"start execution: {node_used=}, {output_dir=}")
            # Using ThreadPoolExecutor to run commands concurrently
            with ThreadPoolExecutor(max_workers=node_used) as executor:
                futures = [executor.submit(run_command_on_server, hostname, command + trace + ".trace." + str(i), output_dir, hostname == hostnames[0]) for i, hostname in enumerate(hostnames)]

            # Waiting for all futures to complete
            for future in futures:
                future.result()
            print(f"end execution: {node_used=}, {output_dir=}, analyzing results...")
            analyze_results(hostnames, output_dir)

            for count_down in range(70, 0, -10):
                print(f"waiting, {count_down} seconds remaining ...")
                time.sleep(10)

if __name__ == "__main__":
    main()
