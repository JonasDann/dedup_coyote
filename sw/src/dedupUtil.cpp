#include "dedupUtil.hpp"
#include "ibvQpMap.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <vector>
#include <fstream>
#include <stdexcept>

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <boost/thread/thread.hpp>

namespace dedup {
// data loading
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

std::unordered_map<std::string, std::pair<std::string, std::string>> readIpInfo(const std::filesystem::path& file_path) {
    std::unordered_map<std::string, std::pair<std::string, std::string>> records;
    std::ifstream inFile(file_path);
    std::string line;
    std::string token;
    int lineCount = 0;
    while (std::getline(inFile, line)) {
        std::istringstream iss(line);
        std::string hostname, host_cpu_ip, host_fpga_ip;
        std::getline(iss, hostname, ',');
        std::getline(iss, host_cpu_ip, ',');
        std::getline(iss, host_fpga_ip, ',');
        records.emplace(hostname, make_pair(host_cpu_ip, host_fpga_ip));
        lineCount ++;
    }
    std::cout << "Got node ip from file: " << file_path << std::endl;
    return records;
}

// connection establish
// deprecated, use dedupSys::exchangeQps();
std::pair<fpga::ibvQpMap*, fpga::ibvQpMap*> setupConnections(const std::string& local_fpga_ip, const std::vector<rdma_connection_entry>& node_outgoing_connections, const std::vector<rdma_connection_entry>& node_incomming_connections){
    // Step 1: connections as master
    auto target_region = 0;
    auto n_buffer_pages = 1;
    auto qp_exchange_port = 8848;

    boost::asio::thread_pool pool(1 + node_incomming_connections.size()); 
    // boost::asio::thread_pool pool(node_outgoing_connections.size() + node_incomming_connections.size()); 
    // std::cout << node_outgoing_connections.size() << ' ' <<  node_incomming_connections.size() << std::endl;
    // master side
    fpga::ibvQpMap* alive_outgoing_connections = new fpga::ibvQpMap();
    boost::mutex out_results_mutex;
    for(int idx = 0; idx < node_outgoing_connections.size(); idx++){
        // boost::asio::post(pool, [&alive_outgoing_connections, &node_outgoing_connections, &out_results_mutex, idx, target_region, &local_fpga_ip, n_buffer_pages, qp_exchange_port]() {
        //     // boost::lock_guard<boost::mutex> lock(out_results_mutex);
        //     rdma_connection_entry outgoing_connection = node_outgoing_connections[idx];
        //     out_results_mutex.lock();
        //     try {
        //         alive_outgoing_connections->addQpair(outgoing_connection.connection_id, target_region, local_fpga_ip, n_buffer_pages);
        //         out_results_mutex.unlock();
        //     } catch (...) {
        //         out_results_mutex.unlock(); // Ensure the mutex is unlocked in case of an exception
        //         throw; // Re-throw the exception
        //     }
        //     // std::cout << "start exchanging qp with slave " << outgoing_connection.hostname << ", connection id: " << outgoing_connection.connection_id << std::endl;
        //     // std::cout << "host cpu ip: " << outgoing_connection.host_cpu_ip << ", port: " << qp_exchange_port + outgoing_connection.connection_id << std::endl;
        //     alive_outgoing_connections->exchangeQpMaster(qp_exchange_port + outgoing_connection.connection_id);
        // });
        rdma_connection_entry outgoing_connection = node_outgoing_connections[idx];
        alive_outgoing_connections->addQpair(outgoing_connection.connection_id, target_region, local_fpga_ip, n_buffer_pages);
    }
    boost::asio::post(pool, [&alive_outgoing_connections, &node_outgoing_connections, &out_results_mutex, target_region, &local_fpga_ip, n_buffer_pages, qp_exchange_port]() {
        alive_outgoing_connections->exchangeQpMaster(qp_exchange_port);
    });

    sleep(10);
    // slave side
    fpga::ibvQpMap* alive_incomming_connections = new fpga::ibvQpMap();
    boost::mutex in_results_mutex;
    for(int idx = 0; idx < node_incomming_connections.size(); idx++){
        boost::asio::post(pool, [&alive_incomming_connections, &node_incomming_connections, &in_results_mutex, idx, target_region, &local_fpga_ip, n_buffer_pages, qp_exchange_port]() {
            // boost::lock_guard<boost::mutex> lock(in_results_mutex);
            rdma_connection_entry incomming_connection = node_incomming_connections[idx];
            in_results_mutex.lock();
            try {
                alive_incomming_connections->addQpair(incomming_connection.connection_id, target_region, local_fpga_ip, n_buffer_pages);
                in_results_mutex.unlock();
            } catch (...) {
                in_results_mutex.unlock(); // Ensure the mutex is unlocked in case of an exception
                throw; // Re-throw the exception
            }
            // std::cout << "start exchanging qp with master " << incomming_connection.hostname << ", connection id: " << incomming_connection.connection_id << std::endl;
            // std::cout << "host cpu ip: " << incomming_connection.host_cpu_ip << ", port: " << qp_exchange_port + incomming_connection.connection_id << std::endl;
            alive_incomming_connections->exchangeQpSlave(incomming_connection.host_cpu_ip.c_str(), incomming_connection.connection_id, qp_exchange_port);
        });
    }

    pool.join();
    // std::cout << "alive outgoing, position 3 " << alive_outgoing_connections.qpairs.size()<< std::endl; 
    return make_pair(alive_outgoing_connections, alive_incomming_connections);
    // boost::mutex out_results_mutex;
    // boost::asio::thread_pool out_pool(node_outgoing_connections.size()); 

    // for(int idx = 0; idx < node_outgoing_connections.size(); idx++){
    //     boost::asio::post(out_pool, [&alive_outgoing_connections, &node_outgoing_connections, &out_results_mutex, idx, target_region, &local_fpga_ip, n_buffer_pages, qp_exchange_port]() {
    //         rdma_connection_entry outgoing_connection = node_outgoing_connections[idx];
    //         alive_outgoing_connections.addQpair(outgoing_connection.connection_id, target_region, local_fpga_ip, n_buffer_pages);
    //         alive_outgoing_connections.exchangeQpMaster(qp_exchange_port) : ictx.exchangeQpSlave(tcp_mstr_ip.c_str(), qp_exchange_port);

    //         int result = concurrentFunction(sharedConfig, i);
    //         boost::lock_guard<boost::mutex> lock(results_mutex);
    //         results.push_back(result);
    //     });
    // }

    // ictx.addQpair(qpId, target_region, local_fpga_ip, n_buffer_pages);
    // mstr ? ictx.exchangeQpMaster(qp_exchange_port) : ictx.exchangeQpSlave(tcp_mstr_ip.c_str(), qp_exchange_port);
    // ibvQpConn *iqp = ictx.getQpairConn(qpId);
    // cProcess *cproc = iqp->getCProc();
    
    
//     ictx.addQpair(qpId, target_region, local_fpga_ip, n_buffer_pages);
//     mstr ? ictx.exchangeQpMaster(qp_exchange_port) : ictx.exchangeQpSlave(tcp_mstr_ip.c_str(), qp_exchange_port);
//     ibvQpConn *iqp = ictx.getQpairConn(qpId);
//     cProcess *cproc = iqp->getCProc();
}


// utils for instr gen
void * set_nop(void * startPtr){
    uint32_t * startPtrUInt32 = (uint32_t *) startPtr;
    startPtrUInt32[0]  = 0;
    startPtrUInt32[1]  = 0;
    startPtrUInt32[2]  = 0;
    startPtrUInt32[3]  = 0;
    startPtrUInt32[4]  = 0;
    startPtrUInt32[5]  = 0;
    startPtrUInt32[6]  = 0;
    startPtrUInt32[7]  = 0;
    startPtrUInt32[8]  = 0;
    startPtrUInt32[9]  = 0;
    startPtrUInt32[10] = 0;
    startPtrUInt32[11] = 0;
    startPtrUInt32[12] = 0;
    startPtrUInt32[13] = 0;
    startPtrUInt32[14] = 0;
    startPtrUInt32[15] = 0;
    return (void *) (startPtrUInt32 + 16);
}

// instr = 512 bit = 32bit x 16
void * set_write_instr(void * startPtr, int startLBA, int LBALen, bool printEn = false){
    uint32_t * startPtrUInt32 = (uint32_t *) startPtr;
    startPtrUInt32[0]  = LBALen;
    startPtrUInt32[1]  = startLBA;
    startPtrUInt32[2]  = 77;
    startPtrUInt32[3]  = 0;
    startPtrUInt32[4]  = 0;
    startPtrUInt32[5]  = 0;
    startPtrUInt32[6]  = 0;
    startPtrUInt32[7]  = 0;
    startPtrUInt32[8]  = 0;
    startPtrUInt32[9]  = 0;
    startPtrUInt32[10] = 0;
    startPtrUInt32[11] = 0;
    startPtrUInt32[12] = 0;
    startPtrUInt32[13] = 0;
    startPtrUInt32[14] = 0;
    startPtrUInt32[15] = 1 << 30;
    if(printEn){
      std::cout << "Instr: write, startLBA:" << startLBA << "\tLBALen:" << LBALen << std::endl;
    }
    return (void *) (startPtrUInt32 + 16);
}

void * set_erase_instr(void * startPtr, uint32_t * sha3Val, bool printEn = false){
    uint32_t * startPtrUInt32 = (uint32_t *) startPtr;
    memcpy(startPtrUInt32, (void*) sha3Val, 32);
    startPtrUInt32[9]  = 0;
    startPtrUInt32[10] = 0;
    startPtrUInt32[11] = 0;
    startPtrUInt32[12] = 0;
    startPtrUInt32[13] = 0;
    startPtrUInt32[14] = 0;
    startPtrUInt32[15] = 2 << 30;
    if(printEn){
      std::stringstream SHA3sstream;
      for (int sha3PieceIdx = 7; sha3PieceIdx >= 0; sha3PieceIdx --){
        SHA3sstream << std::setfill ('0') << std::setw(sizeof(uint32_t)*2) << std::hex << sha3Val[sha3PieceIdx];
      }
      std::cout << "Instr: erase, SHA3:" << SHA3sstream.str() << std::endl;
    }
    return (void *) (startPtrUInt32 + 16);
}

void * set_read_instr(void * startPtr, uint32_t * sha3Val, bool printEn = false){
    uint32_t * startPtrUInt32 = (uint32_t *) startPtr;
    memcpy(startPtrUInt32, (void*) sha3Val, 32);
    startPtrUInt32[9]  = 0;
    startPtrUInt32[10] = 0;
    startPtrUInt32[11] = 0;
    startPtrUInt32[12] = 0;
    startPtrUInt32[13] = 0;
    startPtrUInt32[14] = 0;
    startPtrUInt32[15] = 3 << 30;
    if(printEn){
      std::stringstream SHA3sstream;
      for (int sha3PieceIdx = 7; sha3PieceIdx >= 0; sha3PieceIdx --){
        SHA3sstream << std::setfill ('0') << std::setw(sizeof(uint32_t)*2) << std::hex << sha3Val[sha3PieceIdx];
      }
      std::cout << "Instr: read, SHA3:" << SHA3sstream.str() << std::endl;
    }
    return (void *) (startPtrUInt32 + 16);
}

bool parse_response(vector<uint32_t> &pg_idx_lst, void* rspMem, int* goldenPgIsExec, int* goldenPgRefCount, int* goldenPgIdx, int goldenOpCode, ofstream& outfile) {
  bool allPassed = true;
  uint32_t* rspMemUInt32 = (uint32_t*) rspMem;
  for (int i = 0; i < pg_idx_lst.size(); i++) {
    uint32_t refCount     = rspMemUInt32[i*16 + 8];
    uint32_t SSDLBA       = rspMemUInt32[i*16 + 9];
    uint32_t nodeIdx      = rspMemUInt32[i*16 + 10];
    uint32_t hostLBAStart = rspMemUInt32[i*16 + 11];
    uint32_t hostLBALen   = rspMemUInt32[i*16 + 12];
    uint32_t dataNodeIdx  = rspMemUInt32[i*16 + 13];
    uint32_t execStatus   = rspMemUInt32[i*16 + 15];
    bool isExec = (execStatus & (1 << 29)) ? true : false; // 1 -> op exec -> new page -> not exist(or GC)
    uint32_t opCode = (execStatus>>30);

    // if(nodeIdx != 7){
    //   cout << "single board test should get same node index" << endl;
    // }
    uint32_t goldenDataNodeIdx = (goldenOpCode == 1) ? 77 : 0;
    bool pagePassed = true;
    pagePassed = pagePassed && (refCount == goldenPgRefCount[pg_idx_lst[i]]);
    pagePassed = pagePassed && (hostLBAStart == goldenPgIdx[pg_idx_lst[i]]);
    pagePassed = pagePassed && (hostLBALen == 1);
    pagePassed = pagePassed && (dataNodeIdx == goldenDataNodeIdx);
    pagePassed = pagePassed && (isExec == goldenPgIsExec[pg_idx_lst[i]]);
    pagePassed = pagePassed && (opCode == goldenOpCode);

    allPassed = allPassed && pagePassed;

    std::stringstream SHA3sstream;
    for (int sha3PieceIdx = 7; sha3PieceIdx >= 0; sha3PieceIdx --){
      SHA3sstream << std::setfill ('0') << std::setw(sizeof(uint32_t)*2) << std::hex << rspMemUInt32[i*16 + sha3PieceIdx];
    }

    // write to file
    outfile << "page: " << i << ", at SSD LBA: " << SSDLBA << endl;
    outfile << "overall checking:" << pagePassed << endl;
    outfile << "refCount     " << refCount     << "\texpected " << goldenPgRefCount[pg_idx_lst[i]] << std::endl;
    outfile << "hostLBAStart " << hostLBAStart << "\texpected " << goldenPgIdx[pg_idx_lst[i]]      << std::endl;
    outfile << "hostLBALen   " << hostLBALen   << "\texpected " << 1                               << std::endl;
    outfile << "isExec       " << isExec       << "\texpected " << goldenPgIsExec[pg_idx_lst[i]]   << std::endl;
    outfile << "opCode       " << opCode       << "\texpected " << goldenOpCode                    << std::endl;
    outfile << "SHA3         " << SHA3sstream.str() << endl;
    outfile << endl;
  }
  return allPassed;
}

}