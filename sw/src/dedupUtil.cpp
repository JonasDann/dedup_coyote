#include "dedupUtil.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <vector>
#include <fstream>
#include <stdexcept>

namespace dedup {
std::filesystem::path findLatestSubdirectory(const std::filesystem::path& directory) {
    std::filesystem::path latest_dir;
    std::tm tm = {};
    std::chrono::system_clock::time_point latest_time;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_directory()) {
            std::istringstream ss(entry.path().filename().string());
            ss >> std::get_time(&tm, "%Y_%m%d_%H%M_%S");
            if (ss.fail()) {
                continue; // Skip if the filename is not in the expected format
            }
            auto current_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            if (current_time > latest_time) {
                latest_time = current_time;
                latest_dir = entry.path();
            }
        }
    }

    return latest_dir;
}

std::vector<routing_table_entry> readRoutingTable(const std::filesystem::path& search_directory, const std::string& hostname) {
    std::vector<routing_table_entry> records;
    for (const auto& entry : std::filesystem::directory_iterator(search_directory)) {
        if (!entry.is_regular_file()) continue;
        const auto& file_path = entry.path();
        const auto& filename = file_path.filename().string();
        if (filename.find("local_config_") != std::string::npos && filename.find("_" + hostname + ".csv") != std::string::npos) {
            std::ifstream inFile(file_path);
            std::string line;
            std::string token;
            int lineCount = 0;
            while (std::getline(inFile, line)) {
                std::istringstream iss(line);
                routing_table_entry record;
                std::getline(iss, record.hostname, ',');
                std::getline(iss, token, ',');
                record.node_id = std::stoul(token);
                std::getline(iss, token, ',');
                record.hash_start = std::stoul(token);
                std::getline(iss, token, ',');
                record.hash_len = std::stoul(token);
                std::getline(iss, token, ',');
                record.connection_id = std::stoul(token);
                std::getline(iss, token, ',');
                record.node_idx = std::stoul(token);
                std::getline(iss, record.host_cpu_ip, ',');
                std::getline(iss, record.host_fpga_ip, ',');
                if (lineCount == 0 && record.hostname != hostname) {
                    throw std::runtime_error("Hostname mismatch in routing table");
                }
                records.push_back(record);
                lineCount ++;
            }
            std::cout << "Got routing table from file: " << file_path << std::endl;
            break;
        }
    }
    return records;
}

std::vector<rdma_connection_entry> readRdmaConnections(const std::filesystem::path& search_directory, const std::string& hostname, const std::string& direction) {
    std::vector<rdma_connection_entry> records;
    for (const auto& entry : std::filesystem::directory_iterator(search_directory)) {
        if (!entry.is_regular_file()) continue;
        const auto& file_path = entry.path();
        const auto& filename = file_path.filename().string();
        if (filename.find("rdma_" + direction + "_") != std::string::npos && filename.find("_" + hostname + ".csv") != std::string::npos) {
            std::ifstream inFile(file_path);
            std::string line;
            std::string token;
            int lineCount = 0;
            while (std::getline(inFile, line)) {
                std::istringstream iss(line);
                rdma_connection_entry record;
                std::getline(iss, token, ',');
                record.node_idx = std::stoul(token);
                std::getline(iss, record.hostname, ',');
                std::getline(iss, token, ',');
                record.node_id = std::stoul(token);
                std::getline(iss, token, ',');
                record.connection_id = std::stoul(token);
                std::getline(iss, record.host_cpu_ip, ',');
                std::getline(iss, record.host_fpga_ip, ',');
                records.push_back(record);
                lineCount ++;
            }
            std::cout << "Got " + direction + " rdma connections from file: " << file_path << std::endl;
            break;
        }
    }
    return records;
}


}