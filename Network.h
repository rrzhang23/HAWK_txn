#ifndef HAWK_NETWORK_H
#define HAWK_NETWORK_H

#include "commons.h"
#include "SafeQueue.h"

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <memory>
#include <unordered_map>

#include <grpcpp/grpcpp.h>
#include "generated_protos/network.grpc.pb.h"

class Network {
public:
    class HawkServiceImpl final : public hawk::HawkService::Service {
    public:
        HawkServiceImpl(std::shared_ptr<SafeQueue<NetworkMessage>> incomingQueue, NodeId nodeId);

        grpc::Status SendLockRequest(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                     hawk::NetworkMessage* response) override;
        grpc::Status SendLockResponse(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                      hawk::NetworkMessage* response) override;
        grpc::Status SendReleaseLockRequest(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                            hawk::NetworkMessage* response) override;
        grpc::Status SendReleaseLockResponse(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                             hawk::NetworkMessage* response) override;
        grpc::Status SendPAGRequest(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                    hawk::NetworkMessage* response) override;
        grpc::Status SendPAGResponse(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                     hawk::NetworkMessage* response) override;
        grpc::Status SendWFGReport(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                   hawk::NetworkMessage* response) override;
        grpc::Status SendDeadlockResolution(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                            hawk::NetworkMessage* response) override;
        grpc::Status SendAbortTransactionSignal(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                                hawk::NetworkMessage* response) override;
        grpc::Status SendDistributedDetectionInit(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                                  hawk::NetworkMessage* response) override;
        grpc::Status SendZoneDetectionRequest(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                              hawk::NetworkMessage* response) override;
        grpc::Status SendZoneWFGReport(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                       hawk::NetworkMessage* response) override;
        grpc::Status SendCentralWFGReportFromZone(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                                  hawk::NetworkMessage* response) override;
        grpc::Status SendPathPushingProbe(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                          hawk::NetworkMessage* response) override;
        grpc::Status SendClientCollectWFGRequest(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                                 hawk::NetworkMessage* response) override;
        grpc::Status SendClientPrintDeadlockRequest(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                                    hawk::NetworkMessage* response) override;
        grpc::Status SendClientResolveDeadlockRequest(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                                      hawk::NetworkMessage* response) override;
        grpc::Status SendDeadlockReportToClient(grpc::ServerContext* context, const hawk::NetworkMessage* request,
                                                hawk::NetworkMessage* response) override;

    private:
        std::shared_ptr<SafeQueue<NetworkMessage>> incomingQueue_;
        NodeId nodeId_;
    };

    Network(NodeId nodeId, int numNodes, bool isClient = false);
    ~Network();

    bool init();
    void connectToPeers();
    bool connectToServer(NodeId serverNodeId);
    void sendMessage(const NetworkMessage &msg);
    void broadcastTreeAdjustment(NodeId senderId, const std::vector<std::vector<NodeId>> &detectionZones, const std::vector<NodeId>& detectionZoneLeaders = {});
    std::shared_ptr<SafeQueue<NetworkMessage>> getIncomingQueue() { return incomingQueue_; }

    void sendCollectCommand(NodeId targetNodeId);
    void sendPrintCommand();
    void sendAbortCommand(TransactionId victimTransId);
    void receiveAndPrintResponse();

private:
    NodeId nodeId_;
    int numNodes_;
    bool isClient_;

    std::unique_ptr<grpc::Server> server_;
    std::thread serverThread_;
    HawkServiceImpl service_;

    std::unordered_map<NodeId, std::unique_ptr<hawk::HawkService::Stub>> peerStubs_;
    std::unique_ptr<hawk::HawkService::Stub> clientToServerStub_;

    std::shared_ptr<SafeQueue<NetworkMessage>> incomingQueue_;
    std::mutex sendMutex_;

    static void convertToProtoMessage(const NetworkMessage& internal_msg, hawk::NetworkMessage* proto_msg);
    static NetworkMessage convertFromProtoMessage(const hawk::NetworkMessage& proto_msg);

    static void convertWFGToProto(const std::unordered_map<TransactionId, std::vector<TransactionId>>& wfg_data_map,
                                  hawk::NetworkMessage::WFGData* proto_wfg_data);
    static std::unordered_map<TransactionId, std::vector<TransactionId>>
    convertProtoWFGToInternal(const hawk::NetworkMessage::WFGData& proto_wfg_data);

    static void convertDetectionZonesToProto(const std::vector<std::vector<NodeId>>& internal_zones,
                                             const std::vector<NodeId>& internal_leaders,
                                             hawk::NetworkMessage::DetectionZoneInitData* proto_data);
    static void convertProtoDetectionZonesToInternal(const hawk::NetworkMessage::DetectionZoneInitData& proto_data,
                                                     std::vector<std::vector<NodeId>>& internal_zones,
                                                     std::vector<NodeId>& internal_leaders);

    static void convertCyclesToProto(const std::vector<std::vector<TransactionId>>& internal_cycles,
                                     hawk::NetworkMessage::DeadlockReportToClientData* proto_report);
    static std::vector<std::vector<TransactionId>>
    convertProtoCyclesToInternal(const hawk::NetworkMessage::DeadlockReportToClientData& proto_report);

    static hawk::LockMode toProtoLockMode(LockMode mode);
    static LockMode fromProtoLockMode(hawk::LockMode mode);
};

#endif