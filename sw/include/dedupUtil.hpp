#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace dedup {

struct routing_table_entry {
    std::string hostname;
    uint32_t    node_id;
    uint32_t    hash_start;
    uint32_t    hash_len;
    uint32_t    connection_id;
    uint32_t    node_idx;
    std::string host_cpu_ip;
    std::string host_fpga_ip;
};

struct rdma_connection_entry {
    uint32_t    node_idx;
    std::string hostname;
    uint32_t    node_id;
    uint32_t    connection_id;
    std::string host_cpu_ip;
    std::string host_fpga_ip;
};

std::filesystem::path findLatestSubdirectory(const std::filesystem::path& directory);
std::vector<routing_table_entry> readRoutingTable(const std::filesystem::path& search_directory, const std::string& hostname);
std::vector<rdma_connection_entry> readRdmaConnections(const std::filesystem::path& search_directory, const std::string& hostname, const std::string& direction);

} // namespace dedup
