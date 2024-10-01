#pragma once

#include "ibvQpMap.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace dedup {

enum OpCode {
  WRITE,
  ERASE
};

struct Instr {
  OpCode opcode;
  uint32_t lba;
  vector<uint32_t> pg_idx_lst;
};

struct routing_table_entry {
    std::string hostname;
    uint32_t    node_id;
    uint32_t    hash_start;
    uint32_t    hash_len;
    uint32_t    connection_id;
    uint32_t    node_idx;
    std::string host_cpu_ip;
    std::string host_fpga_ip;
    void print() {
        std::cout << hostname << ", ";
        std::cout << node_id << ", ";
        std::cout << hash_start << ", ";
        std::cout << hash_len << ", ";
        std::cout << connection_id << ", ";
        std::cout << node_idx << ", ";
        std::cout << host_cpu_ip << ", ";
        std::cout << host_fpga_ip << std::endl;
    }
};

struct rdma_connection_entry {
    uint32_t    node_idx;
    std::string hostname;
    uint32_t    node_id;
    uint32_t    connection_id;
    std::string host_cpu_ip;
    std::string host_fpga_ip;
    void print() {
        std::cout << node_idx << ", ";
        std::cout << hostname << ", ";
        std::cout << node_id << ", ";
        std::cout << connection_id << ", ";
        std::cout << host_cpu_ip << ", ";
        std::cout << host_fpga_ip << std::endl;
    }
};

std::filesystem::path findLatestSubdirectory(const std::filesystem::path& directory);
std::vector<routing_table_entry> readRoutingTable(const std::filesystem::path& search_directory, const std::string& hostname);
std::vector<rdma_connection_entry> readRdmaConnections(const std::filesystem::path& search_directory, const std::string& hostname, const std::string& direction);
std::unordered_map<std::string, std::pair<std::string, std::string>> readIpInfo(const std::filesystem::path& file_path);
std::pair<fpga::ibvQpMap*, fpga::ibvQpMap*> setupConnections(const std::string& local_fpga_ip, const std::vector<rdma_connection_entry>& node_outgoing_connections, const std::vector<rdma_connection_entry>& node_incomming_connections);

void * set_nop(void * startPtr);
void * set_write_instr(void * startPtr, int startLBA, int LBALen, bool printEn);
void * set_erase_instr(void * startPtr, uint32_t * sha3Val, bool printEn);
void * set_read_instr(void * startPtr, uint32_t * sha3Val, bool printEn);
bool parse_response(vector<Instr> &instrs, void* rspMem, int* goldenPgIsExec, int* goldenPgRefCount, int* goldenPgIdx, ofstream& outfile);

} // namespace dedup
