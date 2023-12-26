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
    static const int routing_channel_count = 8;
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
        RDMA_QPN_TABLE_BASE = 13 + routing_channel_count * 3
    };
    static const auto msgNAck = 0;
    static const auto msgAck = 1;
    // config for networking
    uint16_t listen_port = 8848;
    uint32_t node_count;
    int32_t max_retry_count = 3;
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
    // network connection
    void initializeNetworkInfo(std::filesystem::path base_path = "/home/jiayli/projects/coyote-rdma", std::filesystem::path ip_path = "server_ip.csv", std::filesystem::path config_path = "rdma_connection");
    void setupConnections();
    // get methods for QPairs
    void addQpair(uint32_t qpid, int32_t vfid, string ip_addr, uint32_t n_pages, std::string direction);
    void removeQpair(uint32_t qpid, std::string direction);
    ibvQpConn* getQpairConn(uint32_t qpid, std::string direction);
    // QP exchange
    void exchangeQps();
    // sync
    void syncBarrier();
    // get methods
    std::string getNodeHostname() { return this->node_host_name; }
    // message sending
    void sendMessage(int socket, const dedup::Message& message);
    dedup::Message receiveMessage(int socket);

private:
    void setupConnectionServer();
    void setupConnectionClient();

    void exchangeQpMaster();
    void exchangeQpSlave(std::string master_hostname, uint32_t qpid);
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
