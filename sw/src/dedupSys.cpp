#include "dedupSys.hpp"
#include "dedupUtil.hpp"
#include "ibvQpConn.hpp"

#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <netdb.h>
#include <sys/socket.h>
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <unordered_map>

namespace dedup {
void dedupSys::sendMessage(int socket, const dedup::Message& message) {
    std::string msgStr = message.toString();
    // std::cout << "message sent: " + msgStr << std::endl;
    send(socket, msgStr.c_str(), msgStr.size(), 0);
    // Add error handling for the send operation
}

dedup::Message dedupSys::receiveMessage(int socket) {
    char buffer[1024];
    std::memset(buffer, 0, 1024);
    ssize_t bytesReceived = recv(socket, buffer, 1024, 0);
    return dedup::Message::fromString(std::string(buffer, bytesReceived));
}

void dedupSys::addQpair(uint32_t qpid, int32_t vfid, string ip_addr, uint32_t n_pages, std::string direction) {
    std::unordered_map<uint32_t, std::unique_ptr<ibvQpConn>> *qp_map;
    if (direction == "m2s") {
        qp_map = &m2s_qpairs;
    } else if (direction == "s2m") {
        qp_map = &s2m_qpairs;
    } else {
        throw std::runtime_error("Invalid direction: " + direction);
    }

    if(qp_map->find(qpid) != qp_map->end())
        throw std::runtime_error("Queue pair already exists");

    auto qpair = std::make_unique<ibvQpConn>(vfid, ip_addr, n_pages);
    qp_map->emplace(qpid, std::move(qpair));
    DBG1("Queue pair created, qpid: " << qpid);
} 

void dedupSys::removeQpair(uint32_t qpid, std::string direction) {
    std::unordered_map<uint32_t, std::unique_ptr<ibvQpConn>> *qp_map;
    if (direction == "m2s") {
        qp_map = &m2s_qpairs;
    } else if (direction == "s2m") {
        qp_map = &s2m_qpairs;
    } else {
        throw std::runtime_error("Invalid direction: " + direction);
    }
    if(qp_map->find(qpid) != qp_map->end())
        qp_map->erase(qpid);
}

ibvQpConn* dedupSys::getQpairConn(uint32_t qpid, std::string direction) {
    std::unordered_map<uint32_t, std::unique_ptr<ibvQpConn>> *qp_map;
    if (direction == "m2s") {
        qp_map = &m2s_qpairs;
    } else if (direction == "s2m") {
        qp_map = &s2m_qpairs;
    } else {
        throw std::runtime_error("Invalid direction: " + direction);
    }
    if(qp_map->find(qpid) != qp_map->end())
        return (*qp_map)[qpid].get();

    return nullptr;
}

// initialize with pre-computed network connection info
void dedupSys::initializeNetworkInfo(std::filesystem::path base_path, std::filesystem::path ip_path, std::filesystem::path config_path){
    // step 1: get self hostname
    const auto hostname_long = boost::asio::ip::host_name();
    // Extract the substring before the first dot for hostname: hostname_long = alveo-u55c_xx.inf.ethz.ch
    std::string hostname = hostname_long.substr(0, hostname_long.find('.'));
    this->node_host_name = hostname;
    // step 2: read all ips
    this->all_nodes_network_info = readIpInfo(base_path / ip_path);
    this->node_count = this->all_nodes_network_info.size();
    this->node_cpu_ip = this->all_nodes_network_info[this->node_host_name].first;
    this->node_fpga_ip = this->all_nodes_network_info[this->node_host_name].second;
    // step 3: get config
    // find latest config path
    std::filesystem::path config_search_path = base_path / config_path;
    auto latest_config_dir = findLatestSubdirectory(config_search_path);
    if (!latest_config_dir.empty()) {
        std::cout << "Latest config under: " << latest_config_dir << std::endl;
    } else {
        std::cout << "No valid config found: " << latest_config_dir << std::endl;
    }
    // get routing table
    this->node_routing_table = dedup::readRoutingTable(latest_config_dir, hostname);
    if (node_routing_table.size() > routing_channel_count){
        throw std::runtime_error("Routing table too big and exceeds the hardware routing channel count");
    }
    // for (auto & iter : node_routing_table){
    //   std::cout << iter.hostname << std::endl;
    //   std::cout << iter.node_id << std::endl;
    //   std::cout << iter.hash_start << std::endl;
    //   std::cout << iter.hash_len << std::endl;
    //   std::cout << iter.connection_id << std::endl;
    //   std::cout << iter.node_idx << std::endl;
    //   std::cout << iter.host_cpu_ip << std::endl;
    //   std::cout << iter.host_fpga_ip << std::endl;
    //   std::cout << std::endl;
    // }
    // get rdma connection information
    this->node_outgoing_connections = dedup::readRdmaConnections(latest_config_dir, hostname, "m2s");
    // for (auto & iter : node_outgoing_connections){
    //   std::cout << iter.node_idx << std::endl;
    //   std::cout << iter.hostname << std::endl;
    //   std::cout << iter.node_id << std::endl;
    //   std::cout << iter.connection_id << std::endl;
    //   std::cout << iter.host_cpu_ip << std::endl;
    //   std::cout << iter.host_fpga_ip << std::endl;
    //   std::cout << std::endl;
    // }
   this->node_incomming_connections = dedup::readRdmaConnections(latest_config_dir, hostname, "s2m");
    // for (auto & iter : node_incomming_connections){
    //   std::cout << iter.node_idx << std::endl;
    //   std::cout << iter.hostname << std::endl;
    //   std::cout << iter.node_id << std::endl;
    //   std::cout << iter.connection_id << std::endl;
    //   std::cout << iter.host_cpu_ip << std::endl;
    //   std::cout << iter.host_fpga_ip << std::endl;
    //   std::cout << std::endl;
    // }
    //step 4. initialize rdma qpairs
    auto target_region = 0;
    auto n_buffer_pages = 1;
    for(const auto& outgoing_connection : node_outgoing_connections){
        this->addQpair(outgoing_connection.connection_id, target_region, this->node_fpga_ip, n_buffer_pages, "m2s");
    }

    for(const auto& incomming_connection : node_incomming_connections){
        this->addQpair(incomming_connection.connection_id, target_region, this->node_fpga_ip, n_buffer_pages, "s2m");
    }

}

// cpu all to all tcp connection setup
void dedupSys::setupConnectionServer() {
    int retry_count = 0;
    int serverSocket = -1;
    DBG2("Server side started ...");
    while (retry_count < this->max_retry_count) {
        try {
            struct sockaddr_in server;
            char recv_buf[recvBuffSize];
            memset(recv_buf, 0, recvBuffSize);

            serverSocket = ::socket(AF_INET, SOCK_STREAM, 0);
            if (serverSocket == -1) 
                throw std::runtime_error("Could not create a socket");

            server.sin_family = AF_INET;
            server.sin_addr.s_addr = INADDR_ANY;
            server.sin_port = htons(this->listen_port);

            if (::bind(serverSocket, (struct sockaddr*)&server, sizeof(server)) < 0)
                throw std::runtime_error("Could not bind a socket");

            if (serverSocket < 0 )
                throw std::runtime_error("Could not listen to a port: " + to_string(this->listen_port));

            // Listen for slave conns
            listen(serverSocket, this->node_count - 1);
            std::cout << "Successfully started listening to port: " << this->listen_port<< ", in try: " << retry_count << "/" << this->max_retry_count << std::endl;
            break; // Successfully started listening, break the retry loop
        } catch (const std::runtime_error& e) {
            std::cout << "Listen to port try: " << retry_count << "/" << this->max_retry_count << ". Error: " << e.what() << std::endl;
            retry_count++;
            if (serverSocket != -1) {
                ::close(serverSocket);
                serverSocket = -1;
            }
            sleep(10);
        }
    }
    if (retry_count == this->max_retry_count) {
        throw std::runtime_error("Failed to start listening on port after retries");
    }
    for(int i = 0; i < this->node_count - 1; i++) {
        int connfd = ::accept(serverSocket, NULL, NULL);
        if (connfd < 0) 
            throw std::runtime_error("Accept failed");

        // Receive the introductory message from the client
        Message introMessage = this->receiveMessage(connfd);
        // std::cout << "receive message: " << introMessage.toString() << std::endl;
        // Ensure the message is of Intro type
        if (introMessage.getType() != Message::Type::SELF_INTRO) {
            // Handle unexpected message type, such as by logging or sending an error message
            ::close(connfd);
            throw std::runtime_error("Received wrong message: " + introMessage.toString());
        }

        std::string client_hostname = introMessage.getContent();
        if (this->client_sockets.find(client_hostname) != this->client_sockets.end()) {
            // Send a message to the client indicating the connection is being closed due to duplicate
            ::close(connfd);
            throw std::runtime_error("Double connections from one client");
        } else {
            this->client_sockets.emplace(client_hostname, connfd); // Store the new connection
        }
    }

    // Close the listening socket as it's no longer needed
    ::close(serverSocket);
    serverSocket = -1;

}

void dedupSys::setupConnectionClient() {
    DBG2("Client side started ...");
    for (const auto& node_info : this->all_nodes_network_info) {
        std::string server_hostname = node_info.first;
        if (server_hostname == this->node_host_name){
            continue;
        }
        std::string dest_addr = node_info.second.first;

        struct addrinfo hints = {}, *res, *t;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        char* service;
        int sockfd = -1;
        uint16_t dest_port = this->listen_port;

        if (asprintf(&service, "%d", dest_port) < 0)
            throw std::runtime_error("asprintf() failed");

        int retry_count = 0;
        while (retry_count < this->max_retry_count) {
            try {
                int n = getaddrinfo(dest_addr.c_str(), service, &hints, &res);
                if (n < 0) {
                    free(service);
                    throw std::runtime_error("getaddrinfo() failed " + dest_addr);
                }
                for (t = res; t; t = t->ai_next) {
                    sockfd = ::socket(t->ai_family, t->ai_socktype, t->ai_protocol);
                    if (sockfd >= 0) {
                        if (!::connect(sockfd, t->ai_addr, t->ai_addrlen)) {
                            Message introMsg(Message::Type::SELF_INTRO, this->node_host_name);
                            this->sendMessage(sockfd, introMsg);
                            break;
                        }
                        ::close(sockfd);
                        sockfd = -1;
                    }
                }

                if (sockfd < 0){
                    throw std::runtime_error("Could not connect to master: " + dest_addr + ":" + to_string(dest_port));
                }

                if (this->server_sockets.find(server_hostname) != this->server_sockets.end()) {
                    // Send a message to the client indicating the connection is being closed due to duplicate
                    ::close(sockfd);
                    throw std::runtime_error("Double connections to one server");
                } else {
                    this->server_sockets.emplace(server_hostname, sockfd); // Store the new connection
                }

                if (res) 
                    freeaddrinfo(res);
                std::cout << "Successfully connected to " + server_hostname + ": " + dest_addr + ": " << to_string(dest_port) << ", in try: " << retry_count << "/" << this->max_retry_count << std::endl;
                break;
            } catch (const std::runtime_error& e) {
                std::cout << "Connect to "+ server_hostname +" try: " << retry_count << "/" << this->max_retry_count << ". Error: " << e.what() << std::endl;
                retry_count++;
                if (sockfd != -1) {
                    ::close(sockfd);
                    sockfd = -1;
                }
                sleep(10);
            }
        }
        free(service);
    }
}

void dedupSys::setupConnections(){
    boost::asio::thread_pool pool(2); // Create a thread pool with 2 threads
    // Post tasks to the thread pool
    boost::asio::post(pool, [this] { setupConnectionServer(); });
    sleep(5);
    boost::asio::post(pool, [this] { setupConnectionClient(); });

    pool.join(); // Wait for all tasks in the pool to complete
    // std::cout << "server" << std::endl;
    // for (const auto& x : this->server_sockets){
    //     std::cout << x. first << ", " << x.second << endl;
    // }
    // std::cout << "client" << std::endl;
    // for (const auto& x : this->client_sockets){
    //     std::cout << x. first << ", " << x.second << endl;
    // }
}

// RDMA Q pair exchange
void dedupSys::exchangeQpMaster() {
    uint32_t recv_qpid;
    uint8_t ack;
    uint32_t n;
    char recv_buf[recvBuffSize];
    memset(recv_buf, 0, recvBuffSize);

    DBG2("Master side QP exchange started ...");

    // Listen for slave conns
    int n_m2s_qpairs = m2s_qpairs.size();
    for(int i = 0; i < n_m2s_qpairs; i++) {
        auto outgoing_connection = this->node_outgoing_connections[i];
        std::string slave_hostname = outgoing_connection.hostname;
        int connfd = this->client_sockets[slave_hostname];
        if (connfd < 0) 
            throw std::runtime_error("Accept failed");
        
        // Read qpid
        if (::read(connfd, recv_buf, sizeof(uint32_t)) != sizeof(uint32_t)) {
            ::close(connfd);
            throw std::runtime_error("Could not read a remote qpid");
        }
        memcpy(&recv_qpid, recv_buf, sizeof(uint32_t));

        // Hash
        if(m2s_qpairs.find(recv_qpid) == m2s_qpairs.end()) {
            // Send nack
            ack = msgNAck;
            if (::write(connfd, &ack, 1) != 1)  {
                ::close(connfd);
                throw std::runtime_error("Could not send ack/nack");
            }

            throw std::runtime_error("Queue pair exchange failed, wrong qpid received");
        }

        // Send ack
        ack = msgAck;
        if (::write(connfd, &ack, 1) != 1)  {
            ::close(connfd);
            throw std::runtime_error("Could not send ack/nack");
        }

        // Read a queue
        if (::read(connfd, recv_buf, sizeof(ibvQ)) != sizeof(ibvQ)) {
            ::close(connfd);
            throw std::runtime_error("Could not read a remote queue");
        }

        ibvQpConn *ibv_qpair_conn = m2s_qpairs[recv_qpid].get();
        ibv_qpair_conn->setConnection(connfd);

        ibvQp *qpair = ibv_qpair_conn->getQpairStruct();
        memcpy(&qpair->remote, recv_buf, sizeof(ibvQ));
        DBG2("Qpair ID: " << recv_qpid);
        qpair->local.print("Local ");
        qpair->remote.print("Remote"); 

        // Send a queue
        if (::write(connfd, &qpair->local, sizeof(ibvQ)) != sizeof(ibvQ))  {
            ::close(connfd);
            throw std::runtime_error("Could not write a local queue");
        }

        // Write context and connection
        ibv_qpair_conn->writeContext(listen_port);

        // ARP lookup
        ibv_qpair_conn->doArpLookup();
    }
}

void dedupSys::exchangeQpSlave(std::string master_hostname, uint32_t qpid) {
    uint8_t ack;
    char recv_buf[recvBuffSize];
    memset(recv_buf, 0, recvBuffSize);

    DBG2("Slave side exchange started ...");

    // int n_qpairs = qpairs.size();
    uint32_t curr_qpid = qpid;
    ibvQpConn *curr_qp_conn = this->s2m_qpairs[qpid].get();
    int sockfd = this->server_sockets[master_hostname];
    const char *trgt_addr = this->all_nodes_network_info[master_hostname].first.c_str();
    if (sockfd < 0)
        throw std::runtime_error("Could not connect to master: " + std::string(trgt_addr) + ":" + to_string(listen_port));

    // Send qpid
    if (::write(sockfd, &curr_qpid, sizeof(uint32_t)) != sizeof(uint32_t)) {
        ::close(sockfd);
        throw std::runtime_error("Could not write a qpid");
    }

    // Wait for ack
    if(::read(sockfd, recv_buf, 1) != 1) {
        ::close(sockfd);
        throw std::runtime_error("Could not read ack/nack");
    }
    memcpy(&ack, recv_buf, 1);
    if(ack != msgAck) 
        throw std::runtime_error("Received nack");

    // Send a queue
    if (::write(sockfd, &curr_qp_conn->getQpairStruct()->local, sizeof(ibvQ)) != sizeof(ibvQ)) {
        ::close(sockfd);
        throw std::runtime_error("Could not write a local queue");
    }

    // Read a queue
    if(::read(sockfd, recv_buf, sizeof(ibvQ)) != sizeof(ibvQ)) {
        ::close(sockfd);
        throw std::runtime_error("Could not read a remote queue");
    }

    curr_qp_conn->setConnection(sockfd);

    ibvQp *qpair = curr_qp_conn->getQpairStruct();
    memcpy(&qpair->remote, recv_buf, sizeof(ibvQ));
    DBG2("Qpair ID: " << curr_qpid);
    qpair->local.print("Local ");
    qpair->remote.print("Remote");

    // Write context and connection
    curr_qp_conn->writeContext(listen_port);

    // ARP lookup
    curr_qp_conn->doArpLookup();
}

void dedupSys::exchangeQps(){
    // Step 1: connections as master
    boost::asio::thread_pool pool(1 + node_incomming_connections.size()); 
    // boost::asio::thread_pool pool(node_outgoing_connections.size() + node_incomming_connections.size()); 
    // std::cout << node_outgoing_connections.size() << ' ' <<  node_incomming_connections.size() << std::endl;
    // master side
    boost::asio::post(pool, [this]{ exchangeQpMaster(); });
    // slave side
    for(const auto & incomming_connection : node_incomming_connections){
        boost::asio::post(pool, [this, &incomming_connection]{ exchangeQpSlave(incomming_connection.hostname, incomming_connection.connection_id); });
    }

    pool.join();
}

// sync
bool dedupSys::syncBarrier() {
    boost::asio::thread_pool pool(1 + 1); 
    // Broadcast sync message to all nodes
    boost::asio::post(pool, [this]{ 
        for (const auto& connection : this->client_sockets) {
            Message syncMessage(Message::Type::SYNC, this->node_host_name);
            this->sendMessage(connection.second, syncMessage);
        }
    });
    // Wait for sync messages from all other nodes
    boost::asio::post(pool, [this]{ 
        // Broadcast sync message to all nodes
        for (const auto& connection : this->server_sockets) {
            Message syncMessage = this->receiveMessage(connection.second);
            // std::cout << "receive message: " << syncMessage.toString() << std::endl;
            if (syncMessage.getType() != Message::Type::SYNC) {
                // Handle unexpected message type, such as by logging or sending an error message
                throw std::runtime_error("Received wrong message: " + syncMessage.toString());
            }
            // std::string server_hostname = syncMessage.getContent();
        }
    });
    pool.join();
    return true;
}

// routing table setup
void dedupSys::setRoutingTable(cProcess* cproc){
    uint32_t active_channel_count = this->node_routing_table.size();
    if (active_channel_count > dedupSys::routing_channel_count){
        throw std::runtime_error("Needed routing table size exceeds hardware available size");
    }
    if (this->node_routing_table[0].hostname != this->node_host_name){
        throw std::runtime_error("Invalid routing table, first entry must correspond to local node");
    }
    cproc->setCSR(active_channel_count, static_cast<uint32_t>(CTLR::ACTIVE_CHANNEL_COUNT));
    for (int index = 0 ; index < active_channel_count; index ++){
        // nodeIdx
        cproc->setCSR(this->node_routing_table[index].node_id, static_cast<uint32_t>(static_cast<uint32_t>(CTLR::ROUTING_TABLE_CONTENT_BASE) + 3 * index));
        // start
        cproc->setCSR(this->node_routing_table[index].hash_start, static_cast<uint32_t>(static_cast<uint32_t>(CTLR::ROUTING_TABLE_CONTENT_BASE) + 1 + 3 * index));
        // len
        cproc->setCSR(this->node_routing_table[index].hash_len, static_cast<uint32_t>(static_cast<uint32_t>(CTLR::ROUTING_TABLE_CONTENT_BASE) + 2 + 3 * index));
    }
    for (int index = 0 ; index < active_channel_count - 1; index ++){
        auto connection_id = this->node_routing_table[index + 1].connection_id;
        auto connection_qpn = this->getQpairConn(connection_id, "m2s")->getQpairStruct()->local.qpn;
        cproc->setCSR(connection_qpn, static_cast<uint32_t>(static_cast<uint32_t>(CTLR::RDMA_QPN_TABLE_BASE) + index));
    }
    cproc->setCSR(0, static_cast<uint32_t>(CTLR::ROUTING_MODE));
    cproc->setCSR(1, static_cast<uint32_t>(CTLR::UPDATE_ROUTING_TABLE));
}


}