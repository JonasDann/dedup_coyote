#pragma once

#include "dedupSys.hpp"
#include "dedupUtil.hpp"
#include "ibvQpConn.hpp"

// #include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
// #include <netdb.h>
// #include <sys/socket.h>

using namespace dedup;
namespace dedup {

class Message {
public:
    enum class Type : uint32_t {
        NACK = 0,
        ACK = 1,
        SELF_INTRO = 2, // client will do a self-intro when connect to server
        SYNC = 3,
        // Add more message types here
    };

    Message(Type type, const std::string& content = "")
        : messageType(type), messageContent(content) {}

    Type getType() const { return messageType; }
    std::string getContent() const { return messageContent; }
    static Message fromString(const std::string& str){
        std::istringstream iss(str);
        std::string typeStr;
        std::string content;
        if (!std::getline(iss, typeStr, ':')) {
            throw std::runtime_error("Invalid message format: missing type");
        }
        int type = std::stoi(typeStr);
        std::getline(iss, content); // The rest of the string is the content
        return Message(static_cast<Type>(type), content);
    }
    std::string toString() const{
        std::ostringstream oss;
        oss << static_cast<uint32_t>(messageType) << ":" << messageContent;
        return oss.str();
    }

private:
    Type messageType;
    std::string messageContent;
};

class dedupSys {
public:
    // hard parameters
    static const uint32_t pg_size = 4096; // change me for 16K
    static const uint32_t huge_pg_size = 2 * 1024 * 1024;
    static const uint32_t pg_per_huge_pg = huge_pg_size / pg_size;
    static const uint32_t instr_size = 64; // 64B = 512b per instr
    static const uint32_t instr_per_page = pg_size / instr_size;
    static const uint32_t node_ht_nbucket = (1 << 15); // number of bucket in hash table(per node)
    static const uint32_t node_ht_entry_per_bucket = 8; // number of bucket in hash table(per node)
    static const uint32_t node_ht_size = node_ht_nbucket * node_ht_entry_per_bucket; // number of 64B entries in hash table(per node)

    static const int routing_channel_count = 10;
    enum class CTLR : uint32_t {
        INITEN = 0,
        INITDONE = 1,
        START = 2,
        RDHOSTADDR = 3,
        WRHOSTADDR = 4,
        LEN = 5,
        CNT = 6,
        PID = 7,
        RDDONE = 8,
        WRDONE = 9,
        FACTORTHROU = 10,
        UPDATE_ROUTING_TABLE = 11,
        ACTIVE_CHANNEL_COUNT = 12,
        ROUTING_TABLE_CONTENT_BASE = 13,
        RDMA_QPN_TABLE_BASE = 13 + routing_channel_count * 3,
        ROUTING_MODE = static_cast<uint32_t>(CTLR::RDMA_QPN_TABLE_BASE) + routing_channel_count - 1
    };
    static const auto msgNAck = 0;
    static const auto msgAck = 1;
    // config for networking
    uint16_t listen_port = 8848;
    uint32_t node_count;
    int32_t max_retry_count = 5;
    int32_t recvBuffSize = 1024;
    // networking
    std::unordered_map<std::string, int> client_sockets; // Store established from other nodes, clinet = slave
    std::unordered_map<std::string, int> server_sockets; // Store established to other nodes, server = master
    // RDMA QP
    std::unordered_map<uint32_t, std::unique_ptr<ibvQpConn>> m2s_qpairs; // client_sockets
    std::unordered_map<uint32_t, std::unique_ptr<ibvQpConn>> s2m_qpairs; // server_sockets
    // hostname, e.g. alveo-u55c-06
    std::string node_host_name;
    std::string node_cpu_ip;
    std::string node_fpga_ip;
    // hostname -> {host cpu ip, host fpga ip}
    std::unordered_map<std::string, std::pair<std::string, std::string>> all_nodes_network_info;
    std::vector<routing_table_entry> node_routing_table;
    std::vector<rdma_connection_entry> node_outgoing_connections;
    std::vector<rdma_connection_entry> node_incomming_connections;

public:

    dedupSys () {}
    ~dedupSys() {
        for (auto & connection : client_sockets){
            if (connection.second >= 0){
                ::close(connection.second);
            }
        }
        for (auto & connection : server_sockets){
            if (connection.second >= 0){
                ::close(connection.second);
            }
        }
    }
    // initialize with pre-computed network connection info
    void initializeNetworkInfo(std::filesystem::path base_path = "/home/jiayli/projects/coyote-rdma", std::filesystem::path ip_path = "server_ip.csv", std::filesystem::path config_path = "rdma_connection");
    // cpu all to all tcp connection setup
    void setupConnections();
    // RDMA Q pair exchange
    void exchangeQps();
    // get methods for QPairs
    ibvQpConn* getQpairConn(uint32_t qpid, std::string direction);
    // sync
    bool syncBarrier();
    // get methods
    std::string getNodeHostname() { return this->node_host_name; }
    // system initialization(hash table header)
    static void setSysInit(cProcess* cproc){
        if (!cproc->getCSR(static_cast<uint32_t>(CTLR::INITDONE))){
            std::cout << "System was not initialized, start initialization process" << std::endl;
            cproc->setCSR(1, static_cast<uint32_t>(CTLR::INITEN));
        } else {
            std::cout << "System was initialized, no op" << std::endl;
        }
    }
    static bool getSysInitDone(cProcess* cproc){ return cproc->getCSR(static_cast<uint32_t>(CTLR::INITDONE)); }
    static void waitSysInitDone(cProcess* cproc){
        while(!getSysInitDone(cproc)) {
            // std::cout << "Waiting for initialization of BloomFilter and HashTable for SHA3 values" << std::endl;
            std::cout << "Waiting for HashTable header initialization, sleep 1 second..." << std::endl;
            sleep(1);
        }
    }
    // routing table setup
    void setRoutingTable(cProcess* cproc);
    // instr
    static void setReqStream(cProcess* cproc, void* req_mem_start, uint32_t req_page_num, void* resp_mem_start, uint32_t throu_factor = 0){
        cproc->setCSR(reinterpret_cast<uint64_t>(req_mem_start), static_cast<uint32_t>(CTLR::RDHOSTADDR));
        cproc->setCSR(reinterpret_cast<uint64_t>(resp_mem_start), static_cast<uint32_t>(CTLR::WRHOSTADDR));
        cproc->setCSR(dedupSys::pg_size, static_cast<uint32_t>(CTLR::LEN));
        cproc->setCSR(req_page_num, static_cast<uint32_t>(CTLR::CNT)); // 16 pages in each command batch
        cproc->setCSR(cproc->getCpid(), static_cast<uint32_t>(CTLR::PID));
        // cproc->setCSR(throu_factor, static_cast<uint32_t>(CTLR::FACTORTHROU)); // throughput factor is not used in current HW
    }
    static void setStart(cProcess* cproc){ cproc->setCSR(1, static_cast<uint32_t>(CTLR::START)); }
    static bool getExecDone(cProcess* cproc, uint32_t input_page_num, uint32_t output_page_num, bool verbose = true){
        auto read_done_counter = cproc->getCSR(static_cast<uint32_t>(CTLR::RDDONE));
        auto write_done_counter = cproc->getCSR(static_cast<uint32_t>(CTLR::WRDONE));
        if (verbose){
            std::cout << "read page count: " << read_done_counter << "/" << input_page_num << std::endl;
            std::cout << "write resp count: " << write_done_counter * 16 << "/" << output_page_num << std::endl;
        }
        return (read_done_counter == input_page_num) and ((write_done_counter * 16) == output_page_num);
    }
    static void waitExecDone(cProcess* cproc, uint32_t output_page_num){
        auto target_counter_value = output_page_num / 16;
        while(cproc->getCSR(static_cast<uint32_t>(CTLR::WRDONE)) != target_counter_value);
    }

private:
    void setupConnectionServer();
    void setupConnectionClient();

    void addQpair(uint32_t qpid, int32_t vfid, string ip_addr, uint32_t n_pages, std::string direction);
    void removeQpair(uint32_t qpid, std::string direction);

    void exchangeQpMaster();
    void exchangeQpSlave(std::string master_hostname, uint32_t qpid);
    // message sending
    void sendMessage(int socket, const dedup::Message& message);
    dedup::Message receiveMessage(int socket);
};

// struct routing_table_entry {
//     std::string hostname;
//     uint32_t    node_id;
//     uint32_t    hash_start;
//     uint32_t    hash_len;
//     uint32_t    connection_id;
//     uint32_t    node_idx;
//     std::string host_cpu_ip;
//     std::string host_fpga_ip;
// };

// struct rdma_connection_entry {
//     uint32_t    node_idx;
//     std::string hostname;
//     uint32_t    node_id;
//     uint32_t    connection_id;
//     std::string host_cpu_ip;
//     std::string host_fpga_ip;
// };

// std::filesystem::path findLatestSubdirectory(const std::filesystem::path& directory);
// std::vector<routing_table_entry> readRoutingTable(const std::filesystem::path& search_directory, const std::string& hostname);
// std::vector<rdma_connection_entry> readRdmaConnections(const std::filesystem::path& search_directory, const std::string& hostname, const std::string& direction);
// std::pair<fpga::ibvQpMap*, fpga::ibvQpMap*> setupConnections(const std::string& local_fpga_ip, const std::vector<rdma_connection_entry>& node_outgoing_connections, const std::vector<rdma_connection_entry>& node_incomming_connections);

} // namespace dedup
