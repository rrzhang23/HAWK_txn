#include "Network.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <sstream>

#include <grpcpp/server_builder.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/completion_queue.h>

#include "generated_protos/network.pb.h"
#include "generated_protos/network.grpc.pb.h"

hawk::LockMode Network::toProtoLockMode(LockMode mode) {
    if (mode == LockMode::SHARED) return hawk::LockMode::SHARED;
    if (mode == LockMode::EXCLUSIVE) return hawk::LockMode::EXCLUSIVE;
    return hawk::LockMode::SHARED;
}

LockMode Network::fromProtoLockMode(hawk::LockMode mode) {
    if (mode == hawk::LockMode::SHARED) return LockMode::SHARED;
    if (mode == hawk::LockMode::EXCLUSIVE) return LockMode::EXCLUSIVE;
    return LockMode::SHARED;
}

void Network::convertWFGToProto(const std::unordered_map<TransactionId, std::vector<TransactionId>>& wfg_data_map,
                               hawk::NetworkMessage::WFGData* proto_wfg_data) {
    for (const auto& pair : wfg_data_map) {
        hawk::NetworkMessage::WFGData::PairTransactionIdList* proto_pair = proto_wfg_data->add_wfg_data_pairs();
        proto_pair->set_key(pair.first);
        for (TransactionId tid : pair.second) {
            proto_pair->add_value(tid);
        }
    }
}

std::unordered_map<TransactionId, std::vector<TransactionId>>
Network::convertProtoWFGToInternal(const hawk::NetworkMessage::WFGData& proto_wfg_data) {
    std::unordered_map<TransactionId, std::vector<TransactionId>> internal_wfg_data;
    for (const auto& proto_pair : proto_wfg_data.wfg_data_pairs()) {
        std::vector<TransactionId> values;
        for (TransactionId tid : proto_pair.value()) {
            values.push_back(tid);
        }
        internal_wfg_data[proto_pair.key()] = values;
    }
    return internal_wfg_data;
}

void Network::convertDetectionZonesToProto(const std::vector<std::vector<NodeId>>& internal_zones,
                                          const std::vector<NodeId>& internal_leaders,
                                          hawk::NetworkMessage::DetectionZoneInitData* proto_data) {
    for (const auto& zone : internal_zones) {
        hawk::NetworkMessage::DetectionZoneInitData::NodeList* proto_node_list = proto_data->add_detection_zones();
        for (NodeId nid : zone) {
            proto_node_list->add_nodes(nid);
        }
    }
    for (NodeId leader : internal_leaders) {
        proto_data->add_detection_zone_leaders(leader);
    }
}

void Network::convertProtoDetectionZonesToInternal(const hawk::NetworkMessage::DetectionZoneInitData& proto_data,
                                                  std::vector<std::vector<NodeId>>& internal_zones,
                                                  std::vector<NodeId>& internal_leaders) {
    internal_zones.clear();
    internal_leaders.clear();
    for (const auto& proto_node_list : proto_data.detection_zones()) {
        std::vector<NodeId> zone;
        for (NodeId nid : proto_node_list.nodes()) {
            zone.push_back(nid);
        }
        internal_zones.push_back(zone);
    }
    for (NodeId leader : proto_data.detection_zone_leaders()) {
        internal_leaders.push_back(leader);
    }
}

void Network::convertCyclesToProto(const std::vector<std::vector<TransactionId>>& internal_cycles,
                                  hawk::NetworkMessage::DeadlockReportToClientData* proto_report) {
    for (const auto& cycle : internal_cycles) {
        hawk::NetworkMessage::DeadlockReportToClientData::TransactionList* proto_cycle = proto_report->add_detected_cycles();
        for (TransactionId tid : cycle) {
            proto_cycle->add_transactions(tid);
        }
    }
}

std::vector<std::vector<TransactionId>>
Network::convertProtoCyclesToInternal(const hawk::NetworkMessage::DeadlockReportToClientData& proto_report) {
    std::vector<std::vector<TransactionId>> internal_cycles;
    for (const auto& proto_cycle : proto_report.detected_cycles()) {
        std::vector<TransactionId> cycle;
        for (TransactionId tid : proto_cycle.transactions()) {
            cycle.push_back(tid);
        }
        internal_cycles.push_back(cycle);
    }
    return internal_cycles;
}
void Network::convertToProtoMessage(const NetworkMessage& internal_msg, hawk::NetworkMessage* proto_msg) {
    proto_msg->set_sender_id(internal_msg.senderId);
    proto_msg->set_receiver_id(internal_msg.receiverId);

    switch (internal_msg.type) {
        case NetworkMessageType::LOCK_REQUEST: {
            proto_msg->set_type(hawk::NetworkMessageType::LOCK_REQUEST);
            hawk::NetworkMessage::LockRequestData* data = proto_msg->mutable_lock_request_data();
            data->set_trans_id(internal_msg.transId);
            data->set_resource_id(internal_msg.resId);
            data->set_mode(Network::toProtoLockMode(internal_msg.mode));
            break;
        }
        case NetworkMessageType::LOCK_RESPONSE: {
            proto_msg->set_type(hawk::NetworkMessageType::LOCK_RESPONSE);
            hawk::NetworkMessage::LockResponseData* data = proto_msg->mutable_lock_response_data();
            data->set_trans_id(internal_msg.transId);
            data->set_resource_id(internal_msg.resId);
            data->set_granted(internal_msg.granted);
            break;
        }
        case NetworkMessageType::RELEASE_LOCK_REQUEST:
        case NetworkMessageType::RELEASE_LOCK_RESPONSE: {
            proto_msg->set_type(internal_msg.type == NetworkMessageType::RELEASE_LOCK_REQUEST ?
                                hawk::NetworkMessageType::RELEASE_LOCK_REQUEST :
                                hawk::NetworkMessageType::RELEASE_LOCK_RESPONSE);
            hawk::NetworkMessage::ReleaseLockData* data = proto_msg->mutable_release_lock_data();
            data->set_trans_id(internal_msg.transId);
            data->set_resource_id(internal_msg.resId);
            break;
        }
        case NetworkMessageType::WFG_REPORT:
        case NetworkMessageType::ZONE_WFG_REPORT:
        case NetworkMessageType::CLIENT_COLLECT_WFG_RESPONSE: {
            proto_msg->set_type(static_cast<hawk::NetworkMessageType>(internal_msg.type));
            hawk::NetworkMessage::WFGData* data = proto_msg->mutable_wfg_data();
            if (internal_msg.type == NetworkMessageType::WFG_REPORT || internal_msg.type == NetworkMessageType::CLIENT_COLLECT_WFG_RESPONSE) {
                Network::convertWFGToProto(internal_msg.wfgData, data);
            } else {
                for (const auto& pair : internal_msg.wfgDataPairs) {
                    hawk::NetworkMessage::WFGData::PairTransactionIdList* proto_pair = data->add_wfg_data_pairs();
                    proto_pair->set_key(pair.first);
                    for (TransactionId tid : pair.second) {
                        proto_pair->add_value(tid);
                    }
                }
            }
            break;
        }
        case NetworkMessageType::DEADLOCK_RESOLUTION:
        case NetworkMessageType::ABORT_TRANSACTION_SIGNAL: {
            proto_msg->set_type(static_cast<hawk::NetworkMessageType>(internal_msg.type));
            hawk::NetworkMessage::AbortTransactionData* data = proto_msg->mutable_abort_transaction_data();
            for (TransactionId tid : internal_msg.deadlockedTransactions) {
                data->add_deadlocked_transactions(tid);
            }
            break;
        }
        case NetworkMessageType::PAG_RESPONSE: {
            proto_msg->set_type(hawk::NetworkMessageType::PAG_RESPONSE);
            hawk::NetworkMessage::PagData* data = proto_msg->mutable_pag_data();
            for (const auto& edge : internal_msg.pagEdges) {
                hawk::WFDEdge* proto_edge = data->add_pag_edges();
                proto_edge->set_waiting_trans_id(edge.waitingTransId);
                proto_edge->set_holding_trans_id(edge.holdingTransId);
                proto_edge->set_waiting_node_id(edge.waitingNodeId);
                proto_edge->set_holding_node_id(edge.holdingNodeId);
            }
            break;
        }
        case NetworkMessageType::DISTRIBUTED_DETECTION_INIT: {
            proto_msg->set_type(hawk::NetworkMessageType::DISTRIBUTED_DETECTION_INIT);
            hawk::NetworkMessage::DetectionZoneInitData* data = proto_msg->mutable_detection_zone_init_data();
            Network::convertDetectionZonesToProto(internal_msg.detectionZones, internal_msg.detectionZoneLeaders, data);
            break;
        }
        case NetworkMessageType::CLIENT_RESOLVE_DEADLOCK_REQUEST: {
            proto_msg->set_type(hawk::NetworkMessageType::CLIENT_RESOLVE_DEADLOCK_REQUEST);
            hawk::NetworkMessage::ResolveDeadlockRequestData* data = proto_msg->mutable_resolve_deadlock_request_data();
            data->set_victim_trans_id(internal_msg.victimTransId);
            break;
        }
        case NetworkMessageType::DEADLOCK_REPORT_TO_CLIENT: {
            proto_msg->set_type(hawk::NetworkMessageType::DEADLOCK_REPORT_TO_CLIENT);
            hawk::NetworkMessage::DeadlockReportToClientData* data = proto_msg->mutable_deadlock_report_to_client_data();
            data->set_deadlock_count(internal_msg.deadlockCount);
            Network::convertCyclesToProto(internal_msg.detectedCycles, data);
            break;
        }
        case NetworkMessageType::PATH_PUSHING_PROBE: {
            proto_msg->set_type(hawk::NetworkMessageType::PATH_PUSHING_PROBE);
            hawk::NetworkMessage::PathPushingProbeData* data = proto_msg->mutable_path_pushing_probe_data();
            for (TransactionId tid : internal_msg.path) {
                data->add_path(tid);
            }
            break;
        }
        case NetworkMessageType::ZONE_DETECTION_REQUEST: {
            proto_msg->set_type(hawk::NetworkMessageType::ZONE_DETECTION_REQUEST);
            hawk::NetworkMessage::ZoneDetectionRequestData* data = proto_msg->mutable_zone_detection_request_data();
            data->set_central_node_id(internal_msg.centralNodeId);
            for (NodeId nid : internal_msg.zoneMembers) {
                data->add_zone_members(nid);
            }
            break;
        }
        case NetworkMessageType::CENTRAL_WFG_REPORT_FROM_ZONE: {
            proto_msg->set_type(hawk::NetworkMessageType::CENTRAL_WFG_REPORT_FROM_ZONE);
            hawk::NetworkMessage::CentralWFGReportFromZoneData* data = proto_msg->mutable_central_wfg_report_data();
            for (const auto& pair : internal_msg.wfgDataPairs) {
                hawk::NetworkMessage::WFGData::PairTransactionIdList* proto_pair = data->mutable_wfg_data()->add_wfg_data_pairs();
                proto_pair->set_key(pair.first);
                for (TransactionId tid : pair.second) {
                    proto_pair->add_value(tid);
                }
            }
            for (const auto& cycle : internal_msg.detectedCycles) {
                hawk::NetworkMessage::DeadlockReportToClientData::TransactionList* proto_cycle_list = data->add_detected_cycles();
                for (TransactionId tid : cycle) {
                    proto_cycle_list->add_transactions(tid);
                }
            }
            data->set_reported_deadlock_count(internal_msg.deadlockCount);
            break;
        }
        case NetworkMessageType::UNKNOWN:
            proto_msg->set_type(hawk::NetworkMessageType::UNKNOWN);
            break;
    }
}

NetworkMessage Network::convertFromProtoMessage(const hawk::NetworkMessage& proto_msg) {
    NetworkMessage internal_msg;
    internal_msg.type = static_cast<NetworkMessageType>(proto_msg.type());
    internal_msg.senderId = proto_msg.sender_id();
    internal_msg.receiverId = proto_msg.receiver_id();

    switch (proto_msg.type()) {
        case hawk::NetworkMessageType::LOCK_REQUEST: {
            const auto& data = proto_msg.lock_request_data();
            internal_msg.transId = data.trans_id();
            internal_msg.resId = data.resource_id();
            internal_msg.mode = Network::fromProtoLockMode(data.mode()); 
            break;
        }
        case hawk::NetworkMessageType::LOCK_RESPONSE: {
            const auto& data = proto_msg.lock_response_data();
            internal_msg.transId = data.trans_id();
            internal_msg.resId = data.resource_id();
            internal_msg.granted = data.granted();
            break;
        }
        case hawk::NetworkMessageType::RELEASE_LOCK_REQUEST:
        case hawk::NetworkMessageType::RELEASE_LOCK_RESPONSE: {
            const auto& data = proto_msg.release_lock_data();
            internal_msg.transId = data.trans_id();
            internal_msg.resId = data.resource_id();
            break;
        }
        case hawk::NetworkMessageType::WFG_REPORT:
        case hawk::NetworkMessageType::ZONE_WFG_REPORT:
        case hawk::NetworkMessageType::CLIENT_COLLECT_WFG_RESPONSE: {
            
            if (proto_msg.type() == hawk::NetworkMessageType::WFG_REPORT || proto_msg.type() == hawk::NetworkMessageType::CLIENT_COLLECT_WFG_RESPONSE) {
                internal_msg.wfgData = Network::convertProtoWFGToInternal(proto_msg.wfg_data()); 
            } else { 
                internal_msg.wfgDataPairs.clear();
                for (const auto& proto_pair : proto_msg.wfg_data().wfg_data_pairs()) {
                    std::vector<TransactionId> values;
                    for (TransactionId tid : proto_pair.value()) {
                        values.push_back(tid);
                    }
                    internal_msg.wfgDataPairs.push_back({proto_pair.key(), values});
                }
            }
            break;
        }
        case hawk::NetworkMessageType::DEADLOCK_RESOLUTION:
        case hawk::NetworkMessageType::ABORT_TRANSACTION_SIGNAL: {
            const auto& data = proto_msg.abort_transaction_data();
            for (TransactionId tid : data.deadlocked_transactions()) {
                internal_msg.deadlockedTransactions.push_back(tid);
            }
            break;
        }
        case hawk::NetworkMessageType::PAG_RESPONSE: {
            const auto& data = proto_msg.pag_data();
            for (const auto& proto_edge : data.pag_edges()) {
                WFDEdge edge;
                edge.waitingTransId = proto_edge.waiting_trans_id();
                edge.holdingTransId = proto_edge.holding_trans_id();
                edge.waitingNodeId = proto_edge.waiting_node_id();
                edge.holdingNodeId = proto_edge.holding_node_id();
                internal_msg.pagEdges.push_back(edge);
            }
            break;
        }
        case hawk::NetworkMessageType::DISTRIBUTED_DETECTION_INIT: {
            const auto& data = proto_msg.detection_zone_init_data();
            Network::convertProtoDetectionZonesToInternal(data, internal_msg.detectionZones, internal_msg.detectionZoneLeaders); 
            break;
        }
        case hawk::NetworkMessageType::CLIENT_RESOLVE_DEADLOCK_REQUEST: {
            const auto& data = proto_msg.resolve_deadlock_request_data();
            internal_msg.victimTransId = data.victim_trans_id();
            break;
        }
        case hawk::NetworkMessageType::DEADLOCK_REPORT_TO_CLIENT: {
            const auto& data = proto_msg.deadlock_report_to_client_data();
            internal_msg.deadlockCount = data.deadlock_count();
            internal_msg.detectedCycles = Network::convertProtoCyclesToInternal(data); 
            break;
        }
        case hawk::NetworkMessageType::PATH_PUSHING_PROBE: {
            const auto& data = proto_msg.path_pushing_probe_data();
            for (TransactionId tid : data.path()) {
                internal_msg.path.push_back(tid);
            }
            break;
        }
        case hawk::NetworkMessageType::ZONE_DETECTION_REQUEST: {
            const auto& data = proto_msg.zone_detection_request_data();
            internal_msg.centralNodeId = data.central_node_id();
            for (NodeId nid : data.zone_members()) {
                internal_msg.zoneMembers.push_back(nid);
            }
            break;
        }
        case hawk::NetworkMessageType::CENTRAL_WFG_REPORT_FROM_ZONE: {
            const auto& data = proto_msg.central_wfg_report_data();
            internal_msg.wfgDataPairs.clear();
            for (const auto& proto_pair : data.wfg_data().wfg_data_pairs()) {
                std::vector<TransactionId> values;
                for (TransactionId tid : proto_pair.value()) {
                    values.push_back(tid);
                }
                internal_msg.wfgDataPairs.push_back({proto_pair.key(), values});
            }
            internal_msg.detectedCycles.clear();
            
            for (const auto& proto_cycle_list : data.detected_cycles()) {
                std::vector<TransactionId> cycle;
                for (TransactionId tid : proto_cycle_list.transactions()) {
                    cycle.push_back(tid);
                }
                internal_msg.detectedCycles.push_back(cycle);
            }
            internal_msg.deadlockCount = data.reported_deadlock_count();
            break;
        }
        case hawk::NetworkMessageType::UNKNOWN:
            
            std::cerr << "Network: Received UNKNOWN message type. Sender: " << proto_msg.sender_id() << ", Receiver: " << proto_msg.receiver_id() << std::endl;
            break;
    }
    return internal_msg;
}


Network::HawkServiceImpl::HawkServiceImpl(std::shared_ptr<SafeQueue<NetworkMessage>> incomingQueue, NodeId nodeId)
    : incomingQueue_(incomingQueue), nodeId_(nodeId) {}


#define GRPC_RPC_HANDLER(RPC_NAME) \
    grpc::Status Network::HawkServiceImpl::RPC_NAME(grpc::ServerContext* context, \
                                                    const hawk::NetworkMessage* request, \
                                                    hawk::NetworkMessage* response) { \
        (void)context; /* Suppress unused parameter warning */ \
        (void)response; /* Suppress unused parameter warning */ \
        NetworkMessage internal_msg; \
        try { \
            internal_msg = Network::convertFromProtoMessage(*request); /* Use Network:: */ \
            incomingQueue_->push(internal_msg); \
            std::cout << "Node " << nodeId_ << ": Received " << static_cast<int>(internal_msg.type) \
                      << " from " << internal_msg.senderId << " via gRPC." << std::endl; \
        } catch (const std::exception& e) { \
            std::cerr << "Node " << nodeId_ << ": Error processing RPC " << #RPC_NAME << ": " << e.what() << std::endl; \
            return grpc::Status(grpc::StatusCode::INTERNAL, "Error processing message"); \
        } \
        return grpc::Status::OK; \
    }

GRPC_RPC_HANDLER(SendLockRequest)
GRPC_RPC_HANDLER(SendLockResponse)
GRPC_RPC_HANDLER(SendReleaseLockRequest)
GRPC_RPC_HANDLER(SendReleaseLockResponse)
GRPC_RPC_HANDLER(SendPAGRequest)
GRPC_RPC_HANDLER(SendPAGResponse)
GRPC_RPC_HANDLER(SendWFGReport)
GRPC_RPC_HANDLER(SendDeadlockResolution)
GRPC_RPC_HANDLER(SendAbortTransactionSignal)
GRPC_RPC_HANDLER(SendDistributedDetectionInit)
GRPC_RPC_HANDLER(SendZoneDetectionRequest)
GRPC_RPC_HANDLER(SendZoneWFGReport)
GRPC_RPC_HANDLER(SendCentralWFGReportFromZone)
GRPC_RPC_HANDLER(SendPathPushingProbe)
GRPC_RPC_HANDLER(SendClientCollectWFGRequest)
GRPC_RPC_HANDLER(SendClientPrintDeadlockRequest)
GRPC_RPC_HANDLER(SendClientResolveDeadlockRequest)
GRPC_RPC_HANDLER(SendDeadlockReportToClient)


Network::Network(NodeId nodeId, int numNodes, bool isClient)
    : nodeId_(nodeId), numNodes_(numNodes), isClient_(isClient),
      
      
      incomingQueue_(std::make_shared<SafeQueue<NetworkMessage>>()),
      service_(incomingQueue_, nodeId) {
}

Network::~Network() {
    systemRunning = false; 

    incomingQueue_->push({});     
    if (server_) {
        std::cout << "Node " << nodeId_ << ": Shutting down gRPC server..." << std::endl;
        server_->Shutdown();
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        std::cout << "Node " << nodeId_ << ": gRPC server shut down." << std::endl;
    }
}

void RunServer(std::unique_ptr<grpc::Server> server) {
    server->Wait(); 
}

bool Network::init() {
    std::string server_address = "0.0.0.0:" + std::to_string(BASE_PORT + nodeId_);
    grpc::ServerBuilder builder;
    
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    
    builder.RegisterService(&service_);
    
    server_ = builder.BuildAndStart();
    if (!server_) {
        std::cerr << "Node " << nodeId_ << ": Failed to start gRPC server on " << server_address << std::endl;
        return false;
    }
    std::cout << "Node " << nodeId_ << ": gRPC server listening on " << server_address << std::endl;

    
    serverThread_ = std::thread(RunServer, std::move(server_));

    return true;
}

void Network::connectToPeers() {
    if (isClient_) {
        
        return;
    }

    
    std::this_thread::sleep_for(std::chrono::seconds(2));

    for (int i = 1; i <= numNodes_; ++i) {
        if (i == nodeId_) {
            continue; 
        }
        std::string peer_address = "localhost:" + std::to_string(BASE_PORT + i);
        std::cout << "Node " << nodeId_ << ": Connecting to peer " << i << " at " << peer_address << std::endl;
        
        std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
            peer_address, grpc::InsecureChannelCredentials());
        
        peerStubs_[i] = hawk::HawkService::NewStub(channel);
    }
    std::cout << "Node " << nodeId_ << ": Finished connecting to peers (created stubs)." << std::endl;
}

bool Network::connectToServer(NodeId serverNodeId) {
    if (!isClient_) {
        std::cerr << "Error: connectToServer called on a non-client node." << std::endl;
        return false;
    }

    std::string server_address = "localhost:" + std::to_string(BASE_PORT + serverNodeId);
    std::cout << "Client: Connecting to server " << serverNodeId << " at " << server_address << std::endl;
    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
        server_address, grpc::InsecureChannelCredentials());
    clientToServerStub_ = hawk::HawkService::NewStub(channel);

    
    
    grpc::ClientContext context;
    hawk::NetworkMessage request;
    request.set_type(hawk::NetworkMessageType::CLIENT_PRINT_DEADLOCK_REQUEST);
    request.set_sender_id(0); 
    request.set_receiver_id(serverNodeId);

    hawk::NetworkMessage response;
    grpc::Status status = clientToServerStub_->SendClientPrintDeadlockRequest(&context, request, &response);

    if (status.ok()) {
        std::cout << "Client: Successfully connected to server " << serverNodeId << "." << std::endl;
        
        
        return true;
    } else {
        std::cerr << "Client: Failed to connect to server " << serverNodeId << ": "
                  << status.error_code() << " - " << status.error_message() << std::endl;
        return false;
    }
}

void Network::sendMessage(const NetworkMessage &msg) {
    std::unique_lock<std::mutex> lock(sendMutex_); 

    NodeId targetNodeId = msg.receiverId;
    if (targetNodeId == 0 && !isClient_) { 
                                         
        std::cerr << "Network: Broadcast from server not directly supported by gRPC sendMessage. Please use specific broadcast methods (e.g., broadcastTreeAdjustment)." << std::endl;
        return;
    }

    hawk::NetworkMessage proto_msg;
    Network::convertToProtoMessage(msg, &proto_msg); 

    grpc::ClientContext context;
    hawk::NetworkMessage response; 

    grpc::Status status;

    if (isClient_) {
        if (!clientToServerStub_) {
            std::cerr << "Client: Not connected to server. Cannot send message." << std::endl;
            return;
        }
        
        switch (proto_msg.type()) {
            case hawk::NetworkMessageType::CLIENT_COLLECT_WFG_REQUEST:
                status = clientToServerStub_->SendClientCollectWFGRequest(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::CLIENT_PRINT_DEADLOCK_REQUEST:
                status = clientToServerStub_->SendClientPrintDeadlockRequest(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::CLIENT_RESOLVE_DEADLOCK_REQUEST:
                status = clientToServerStub_->SendClientResolveDeadlockRequest(&context, proto_msg, &response);
                break;
            default:
                std::cerr << "Client: Attempted to send unsupported message type: " << static_cast<int>(proto_msg.type()) << std::endl;
                status = grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Unsupported client message type");
                break;
        }
    } else { 
        auto it = peerStubs_.find(targetNodeId);
        if (it == peerStubs_.end() || !it->second) {
            std::cerr << "Node " << nodeId_ << ": No gRPC stub found for target node " << targetNodeId << std::endl;
            return;
        }
        hawk::HawkService::Stub* stub = it->second.get();

        switch (proto_msg.type()) {
            case hawk::NetworkMessageType::LOCK_REQUEST:
                status = stub->SendLockRequest(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::LOCK_RESPONSE:
                status = stub->SendLockResponse(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::RELEASE_LOCK_REQUEST:
                status = stub->SendReleaseLockRequest(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::RELEASE_LOCK_RESPONSE:
                status = stub->SendReleaseLockResponse(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::WFG_REPORT:
                status = stub->SendWFGReport(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::PAG_REQUEST:
                status = stub->SendPAGRequest(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::PAG_RESPONSE:
                status = stub->SendPAGResponse(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::DEADLOCK_RESOLUTION:
                status = stub->SendDeadlockResolution(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::ABORT_TRANSACTION_SIGNAL:
                status = stub->SendAbortTransactionSignal(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::DISTRIBUTED_DETECTION_INIT:
                status = stub->SendDistributedDetectionInit(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::ZONE_DETECTION_REQUEST:
                status = stub->SendZoneDetectionRequest(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::ZONE_WFG_REPORT:
                status = stub->SendZoneWFGReport(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::CENTRAL_WFG_REPORT_FROM_ZONE:
                status = stub->SendCentralWFGReportFromZone(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::PATH_PUSHING_PROBE:
                status = stub->SendPathPushingProbe(&context, proto_msg, &response);
                break;
            case hawk::NetworkMessageType::DEADLOCK_REPORT_TO_CLIENT: 
                status = stub->SendDeadlockReportToClient(&context, proto_msg, &response);
                break;
            default:
                std::cerr << "Node " << nodeId_ << ": Attempted to send unsupported message type to peer: " << static_cast<int>(proto_msg.type()) << std::endl;
                status = grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Unsupported peer message type");
                break;
        }
    }

    if (!status.ok()) {
        std::cerr << "Node " << nodeId_ << ": RPC failed (Type: " << static_cast<int>(proto_msg.type())
                  << ", Target: " << targetNodeId << "): " << status.error_code() << " - " << status.error_message() << std::endl;
    } else {
    }
}

void Network::broadcastTreeAdjustment(NodeId senderId, const std::vector<std::vector<NodeId>> &detectionZones, const std::vector<NodeId>& detectionZoneLeaders) {
    NetworkMessage msg;
    msg.type = NetworkMessageType::DISTRIBUTED_DETECTION_INIT;
    msg.senderId = senderId;
    msg.receiverId = 0; 

    msg.detectionZones = detectionZones;
    msg.detectionZoneLeaders = detectionZoneLeaders;

    hawk::NetworkMessage proto_msg;
    Network::convertToProtoMessage(msg, &proto_msg); 

    
    for (int i = 1; i <= numNodes_; ++i) {
        if (i == nodeId_) {
            continue; 
        }
        auto it = peerStubs_.find(i);
        if (it == peerStubs_.end() || !it->second) {
            std::cerr << "Node " << nodeId_ << ": No gRPC stub found for broadcast to node " << i << std::endl;
            continue;
        }
        grpc::ClientContext context;
        hawk::NetworkMessage response;
        grpc::Status status = it->second->SendDistributedDetectionInit(&context, proto_msg, &response);
        if (!status.ok()) {
            std::cerr << "Node " << nodeId_ << ": Broadcast to node " << i << " failed: "
                      << status.error_code() << " - " << status.error_message() << std::endl;
        }
    }
    std::cout << "Node " << nodeId_ << ": Broadcasted Tree Adjustment (Detection Init) message." << std::endl;
}


void Network::sendCollectCommand(NodeId targetNodeId) {
    NetworkMessage msg;
    msg.type = NetworkMessageType::CLIENT_COLLECT_WFG_REQUEST;
    msg.senderId = 0; 
    msg.receiverId = targetNodeId;
    sendMessage(msg);
    std::cout << "Client: Sent COLLECT_WFG_REQUEST to Node " << targetNodeId << std::endl;
}

void Network::sendPrintCommand() {
    NetworkMessage msg;
    msg.type = NetworkMessageType::CLIENT_PRINT_DEADLOCK_REQUEST;
    msg.senderId = 0; 
    msg.receiverId = CENTRALIZED_NODE_ID; 
    sendMessage(msg);
    std::cout << "Client: Sent PRINT_DEADLOCK_REQUEST to Node " << CENTRALIZED_NODE_ID << std::endl;
}

void Network::sendAbortCommand(TransactionId victimTransId) {
    NetworkMessage msg;
    msg.type = NetworkMessageType::CLIENT_RESOLVE_DEADLOCK_REQUEST;
    msg.senderId = 0; 
    msg.receiverId = CENTRALIZED_NODE_ID; 
    msg.victimTransId = victimTransId;
    sendMessage(msg);
    std::cout << "Client: Sent RESOLVE_DEADLOCK_REQUEST for Trans " << victimTransId << " to Node " << CENTRALIZED_NODE_ID << std::endl;
}

void Network::receiveAndPrintResponse() {
    std::cout << "Client: Waiting for response..." << std::endl;
    try {
        NetworkMessage response = incomingQueue_->pop(); 

        if (response.type == NetworkMessageType::CLIENT_COLLECT_WFG_RESPONSE) {
            std::cout << "Client: Received aggregated WFG report from Node " << response.senderId << ":\n";
            for (const auto &pair : response.wfgData) { 
                std::cout << "  Trans " << pair.first << " waits for: ";
                for (TransactionId t : pair.second) {
                    std::cout << t << " ";
                }
                std::cout << "\n";
            }
        } else if (response.type == NetworkMessageType::DEADLOCK_REPORT_TO_CLIENT) {
            if (response.detectedCycles.empty()) {
                std::cout << "Client: No deadlocks detected.\n";
            } else {
                std::cout << "Client: Detected " << response.detectedCycles.size() << " deadlock cycles:\n";
                for (const auto &cycle : response.detectedCycles) {
                    std::cout << "  Cycle: ";
                    for (TransactionId tid : cycle) {
                        std::cout << tid << " -> ";
                    }
                    std::cout << cycle[0] << "\n"; 
                }
            }
        } else {
            std::cout << "Client: Received unhandled response type: " << static_cast<int>(response.type) << "\n";
        }
    } catch (const std::runtime_error &e) {
        std::cerr << "Client: Receive and print response loop terminated: " << e.what() << "\n";
    }
}
