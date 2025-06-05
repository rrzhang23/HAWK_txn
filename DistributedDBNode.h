#ifndef HAWK_DISTRIBUTED_DB_NODE_H
#define HAWK_DISTRIBUTED_DB_NODE_H

#include "commons.h"
#include "ResourceManager.h"
#include "TransactionManager.h"
#include "LockTable.h"
#include "DeadlockDetector.h"
#include "PAGManager.h"
#include "DetectionZoneManager.h"
#include "Network.h"
#ifdef TRANSACTION_TYPE_TPCC
#include "tpcc.h"
#include "tpcc_data_generator.h"
#include "tpcc_transaction.h"
#endif // TRANSACTION_TYPE_TPCC
#include <vector>
#include <unordered_map>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <unordered_set>
#include <atomic>

#ifdef TRANSACTION_TYPE_TPCC
#include "tpcc.h"
#endif

class DistributedDBNode
{
public:
    DistributedDBNode(NodeId id, int numNodes, Network &network);
    ~DistributedDBNode();

    void run();

    std::vector<long long> getCompletedTransactionLatencies();

private:
    NodeId nodeId_;
    int numNodes_;
    ResourceManager resourceManager_;
    TransactionManager transactionManager_;
    LockTable lockTable_;
    PAGManager pagManager_;
    DetectionZoneManager detectionZoneManager_;
    Network &network_;
    std::unique_ptr<DeadlockDetector> deadlockDetector_;

    std::thread transactionPollingThread_;
    std::thread messageProcessingThread_;
    std::thread deadlockDetectionThread_;

    std::thread pagSampleThread;
    std::thread treeAdjustThread;
    std::thread distributedDetectCoordinatorThread;
    std::thread centralizedDetectThread;
    std::thread pathPushingThread;

    bool isCentralizedNode_;
    bool isCentralizedDetectionMode_;

    std::unordered_map<TransactionId, std::vector<TransactionId>> aggregatedWfg_;
    std::mutex aggregatedWfgMutex_;
    int wfgReportsReceived_;
    int wfgReportsExpected_;

    std::vector<WFDEdge> aggregatedPagEdges_;
    std::mutex aggregatedPagEdgesMutex_;
    int pagResponsesReceived_;
    int pagResponsesExpected_;

    std::unordered_map<TransactionId, std::vector<TransactionId>> centralAggregatedWfg_;
    std::mutex centralAggregatedWfgMutex_;
    int centralWfgReportsReceived_;
    int centralDeadlockCount_;
    std::vector<std::vector<TransactionId>> centralDetectedCycles_;

    SafeQueue<long long> completedTransactionLatencies_;
    std::chrono::high_resolution_clock::time_point lastReportTime_;

    std::atomic<long long> totalDeadlocksFromZones_;
    std::atomic<long long> totalDeadlocksFromCentral_;

    std::chrono::high_resolution_clock::time_point lastTreeAdjustTime_;
    long long prevTotalDeadlocksFromZones_ = 0;
    long long prevTotalDeadlocksFromCentral_ = 0;


#ifdef TRANSACTION_TYPE_TPCC
    TPCCDatabase tpcc_db_;
    TPCCRandom tpcc_rng_;
    std::thread tpcc_workload_thread_;
#endif // TRANSACTION_TYPE_TPCC

    void transactionPollingLoop();
    void messageProcessingLoop();
    // void deadlockDetectionLoop();

    void pagSamplingLoop();
    void treeAdjustmentLoop();
    void distributedDetectCoordinatorLoop();
    void centralizedDetectLoop();
    void pathPushingDetectionLoop();

    void checkAndResolveDeadlocks(const std::unordered_map<TransactionId, std::vector<TransactionId>> &graph);
    void checkAndResolveDeadlocksForZone(NodeId zoneLeaderId, const std::unordered_map<TransactionId, std::vector<TransactionId>> &graph);
    TransactionId selectVictim(const std::vector<TransactionId> &cycle, const std::unordered_map<TransactionId, int> &transactionFrequencies);

    void handleWFGReport(NodeId reporterNodeId, const std::unordered_map<TransactionId, std::vector<TransactionId>> &wfgData);
    void handlePAGRequest(NodeId requesterNodeId);
    void handlePAGResponse(NodeId reporterNodeId, const std::vector<WFDEdge> &pagEdges);
    void handleDeadlockResolution(const std::vector<TransactionId> &transIdsToAbort);
    void handleAbortTransactionSignal(const std::vector<TransactionId> &transIdsToAbort);

    void handleDistributedDetectionInit(const std::vector<std::vector<NodeId>>& detectionZones, const std::vector<NodeId>& detectionZoneLeaders);
    void handleZoneDetectionRequest(NodeId centralNodeId, const std::vector<NodeId>& zoneMembers);
    void handleZoneWFGReport(NodeId reporterNodeId, const std::vector<std::pair<TransactionId, std::vector<TransactionId>>> &wfgDataPairs);
    void handleCentralWFGReportFromZone(NodeId zoneLeaderId, const std::vector<std::pair<TransactionId, std::vector<TransactionId>>> &wfgDataPairs, const std::vector<std::vector<TransactionId>>& detectedCycles, int reportedDeadlockCount);

    void handlePathPushingProbe(const NetworkMessage& msg);
    void initiatePathPushingProbes();

    void mergeWFG(std::unordered_map<TransactionId, std::vector<TransactionId>>& targetWfg,
                  const std::vector<std::pair<TransactionId, std::vector<TransactionId>>>& sourceData);

    std::vector<std::pair<TransactionId, std::vector<TransactionId>>>
    convertWFGToMessageFormat(const std::unordered_map<TransactionId, std::vector<TransactionId>>& wfg);

    void handleClientCollectWFGRequest(NodeId clientId);
    void handleClientPrintDeadlockRequest(NodeId clientId);
    void handleClientResolveDeadlockRequest(TransactionId victimTransId, NodeId clientId);

#ifdef TRANSACTION_TYPE_TPCC
    void tpccWorkloadLoop();
#endif
};

#endif