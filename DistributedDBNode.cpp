#include "DistributedDBNode.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <numeric>

/**
 * @brief Constructor for the DistributedDBNode class.
 *
 * Initializes a distributed database node. It sets the node ID, references the network instance,
 * and initializes various managers (resource, transaction, lock table, PAG, detection zone).
 * It also sets relevant flags and counters based on the configured deadlock detection mode.
 *
 * @param id The unique identifier of the current node.
 * @param numNodes The total number of nodes in the system.
 * @param network A reference to the network communication instance, used for inter-node message passing.
 */
DistributedDBNode::DistributedDBNode(NodeId id, int numNodes, Network &network)
    : nodeId_(id),
      numNodes_(numNodes),
      resourceManager_(id),
      transactionManager_(id, resourceManager_,
                         network.getIncomingQueue(),
                         [&](const NetworkMessage &msg) { network.sendMessage(msg); }, tpcc_rng_),
      lockTable_(id, resourceManager_, transactionManager_),
      pagManager_(),
      detectionZoneManager_(id),
      network_(network),
      deadlockDetector_(std::make_unique<DeadlockDetector>()),
      isCentralizedNode_(id == CENTRALIZED_NODE_ID),
      isCentralizedDetectionMode_(DEADLOCK_DETECTION_MODE == MODE_CENTRALIZED),
      aggregatedWfg_(),
      wfgReportsReceived_(0),
      wfgReportsExpected_(0),
      aggregatedPagEdges_(),
      pagResponsesReceived_(0),
      pagResponsesExpected_(0),
      centralAggregatedWfg_(),
      centralWfgReportsReceived_(0),
      centralDeadlockCount_(0),
      centralDetectedCycles_(),
      completedTransactionLatencies_(),
      lastReportTime_(std::chrono::high_resolution_clock::now()),
      totalDeadlocksFromZones_(0),
      totalDeadlocksFromCentral_(0),
      lastTreeAdjustTime_(std::chrono::high_resolution_clock::now()),
      prevTotalDeadlocksFromZones_(0),
      prevTotalDeadlocksFromCentral_(0)
{
    transactionPollingThread_ = std::thread(&DistributedDBNode::transactionPollingLoop, this);
    messageProcessingThread_ = std::thread(&DistributedDBNode::messageProcessingLoop, this);

    if (DEADLOCK_DETECTION_MODE == MODE_CENTRALIZED) {
        centralizedDetectThread = std::thread(&DistributedDBNode::centralizedDetectLoop, this);
    } else if (DEADLOCK_DETECTION_MODE == MODE_HAWK) {
        distributedDetectCoordinatorThread = std::thread(&DistributedDBNode::distributedDetectCoordinatorLoop, this);
        pagSampleThread = std::thread(&DistributedDBNode::pagSamplingLoop, this);
        treeAdjustThread = std::thread(&DistributedDBNode::treeAdjustmentLoop, this);
    } else if (DEADLOCK_DETECTION_MODE == MODE_PATH_PUSHING) {
        pathPushingThread = std::thread(&DistributedDBNode::pathPushingDetectionLoop, this);
    }
#ifdef TRANSACTION_TYPE_TPCC
    TPCCDataGenerator data_gen;
    tpcc_db_ = data_gen.generateData(NUM_WAREHOUSES);
    tpcc_workload_thread_ = std::thread(&DistributedDBNode::tpccWorkloadLoop, this);
#endif // TRANSACTION_TYPE_TPCC
}

DistributedDBNode::~DistributedDBNode()
{
    // Set the system running flag to false, notifying all threads to stop
    systemRunning = false;

    // Ensure all threads have stopped and joined
    if (transactionPollingThread_.joinable())
    {
        transactionPollingThread_.join();
    }
    if (messageProcessingThread_.joinable())
    {
        messageProcessingThread_.join();
    }
    if (deadlockDetectionThread_.joinable())
    {
        deadlockDetectionThread_.join();
    }
    std::cout << "Node " << nodeId_ << " server shut down.\\n";
}

#ifdef TRANSACTION_TYPE_TPCC
void DistributedDBNode::tpccWorkloadLoop() {
    std::cout << "Node " << nodeId_ << ": Starting TPC-C workload generation.\n";
    
    int local_warehouse_start_id = (nodeId_ - 1) * WAREHOUSES_PER_NODE + 1;
    int local_warehouse_end_id = local_warehouse_start_id + WAREHOUSES_PER_NODE - 1;

    while (systemRunning) {
        int transaction_type_roll = tpcc_rng_.generateRandomInt(1, 100);

        NodeId home_node_id = nodeId_;

        std::shared_ptr<TPCCTransaction> tpcc_txn;

        if (transaction_type_roll <= 45) {
            int w_id = tpcc_rng_.generateRandomInt(local_warehouse_start_id, local_warehouse_end_id);
            int d_id = tpcc_rng_.generateRandomInt(1, 10);
            int c_id = tpcc_rng_.generateCID();

            std::vector<std::pair<int, int>> item_info;
            int num_items = tpcc_rng_.generateRandomInt(5, 15);
            for (int i = 0; i < num_items; ++i) {
                int item_id = tpcc_rng_.generateItemID();
                int quantity = tpcc_rng_.generateRandomInt(1, 10);
                item_info.push_back({item_id, quantity});
            }
            
            tpcc_txn = std::make_shared<TPCCNewOrderTransaction>(
                tpcc_db_, lockTable_, transactionManager_.getNextTransactionId(),
                home_node_id,
                tpcc_rng_,
                w_id, d_id, c_id, item_info
            );

        } else if (transaction_type_roll <= 88) {
            int w_id = tpcc_rng_.generateRandomInt(local_warehouse_start_id, local_warehouse_end_id);
            int d_id = tpcc_rng_.generateRandomInt(1, 10);
            
            int c_w_id;
            if (tpcc_rng_.generateRandomDouble(0.0, 1.0) < 0.15) {
                c_w_id = tpcc_rng_.generateRandomWarehouseId(home_node_id);
            } else {
                c_w_id = w_id;
            }
            
            int c_d_id = tpcc_rng_.generateRandomInt(1, 10);
            int c_id;
            if (tpcc_rng_.generateRandomInt(1, 100) <= 40) { 
                c_id = 0;
            } else { 
                c_id = tpcc_rng_.generateCID();
            }
            double h_amount = tpcc_rng_.generateRandomDouble(1.00, 5000.00);

            tpcc_txn = std::make_shared<TPCCPaymentTransaction>(
                tpcc_db_, lockTable_, transactionManager_.getNextTransactionId(),
                home_node_id,
                tpcc_rng_,
                w_id, d_id, c_w_id, c_d_id, c_id, h_amount
            );

        } else if (transaction_type_roll <= 92) {
            int w_id = tpcc_rng_.generateRandomInt(local_warehouse_start_id, local_warehouse_end_id);
            int d_id = tpcc_rng_.generateRandomInt(1, 10);
            int c_id;
            if (tpcc_rng_.generateRandomInt(1, 100) <= 40) {
                c_id = 0;
            } else {
                c_id = tpcc_rng_.generateCID();
            }

            tpcc_txn = std::make_shared<TPCCOrderStatusTransaction>(
                tpcc_db_, lockTable_, transactionManager_.getNextTransactionId(),
                home_node_id,
                tpcc_rng_,
                w_id, d_id, c_id
            );

        } else if (transaction_type_roll <= 96) {
            int w_id = tpcc_rng_.generateRandomInt(local_warehouse_start_id, local_warehouse_end_id);
            int o_carrier_id = tpcc_rng_.generateRandomInt(1, 10);

            tpcc_txn = std::make_shared<TPCCDeliveryTransaction>(
                tpcc_db_, lockTable_, transactionManager_.getNextTransactionId(),
                home_node_id,
                tpcc_rng_,
                w_id, o_carrier_id
            );

        } else {
            int w_id = tpcc_rng_.generateRandomInt(local_warehouse_start_id, local_warehouse_end_id);
            int d_id = tpcc_rng_.generateRandomInt(1, 10);
            int threshold = tpcc_rng_.generateRandomInt(10, 20);

            tpcc_txn = std::make_shared<TPCCStockLevelTransaction>(
                tpcc_db_, lockTable_, transactionManager_.getNextTransactionId(),
                home_node_id,
                tpcc_rng_,
                w_id, d_id, threshold
            );
        }

        if (tpcc_txn) {
            transactionManager_.addTPCCTransaction(tpcc_txn);
        }

        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    std::cout << "Node " << nodeId_ << ": TPC-C workload generation stopped.\n";
}
#endif

// all threads are start in DistributedDBNode()
void DistributedDBNode::run() {}

std::vector<long long> DistributedDBNode::getCompletedTransactionLatencies() {
    return completedTransactionLatencies_.drain();
}
/**
 * @brief Transaction polling loop.
 *
 * This thread periodically generates new transactions and executes their SQL statements.
 * For each transaction, it attempts to execute the current SQL statement. If the SQL requires a remote lock,
 * it sends a request and waits for a response via a condition variable.
 * If a transaction completes or aborts, it is removed from the active transaction list.
 */
void DistributedDBNode::transactionPollingLoop()
{
    while (systemRunning)
    {
        if (kTRANSACTION_TYPE_TPCC == 0) {
            int active_txns = transactionManager_.getActiveTransactions().size();
            if (active_txns < MAX_CONCURRENT_TRANSACTIONS_PER_NODE) {
                int num_to_start = MAX_CONCURRENT_TRANSACTIONS_PER_NODE - active_txns;
                for (int i = 0; i < num_to_start; ++i) {
                    transactionManager_.beginTransaction();
                }
            }
        }

        std::unordered_set<TransactionId> currentActiveTxns = transactionManager_.getActiveTransactions();
        for (TransactionId tid : currentActiveTxns) {
            if (!systemRunning) break;
            transactionManager_.tryExecuteNextSQLStatement(tid);
        }
    }
}
/**
 * @brief Message processing loop.
 *
 * This thread continuously pops messages from the network incoming queue and dispatches them
 * to the appropriate handler functions based on the message type.
 * It serves as the entry point for all network communication processing at the node.
 */
void DistributedDBNode::messageProcessingLoop()
{
    while (systemRunning)
    {
        try
        {
            NetworkMessage msg = network_.getIncomingQueue()->pop();

            switch (msg.type)
            {
            case NetworkMessageType::LOCK_REQUEST:
                break;

            case NetworkMessageType::LOCK_RESPONSE:
                transactionManager_.handleSQLResponse(msg.transId, msg.granted, msg.resId);
                break;

            case NetworkMessageType::RELEASE_LOCK_REQUEST:
                break;

            case NetworkMessageType::RELEASE_LOCK_RESPONSE:
                break;

            case NetworkMessageType::WFG_REPORT:
                handleWFGReport(msg.senderId, msg.wfgData);
                break;

            case NetworkMessageType::PAG_REQUEST:
                handlePAGRequest(msg.senderId);
                break;

            case NetworkMessageType::PAG_RESPONSE:
                handlePAGResponse(msg.senderId, msg.pagEdges);
                break;

            case NetworkMessageType::DEADLOCK_RESOLUTION:
                handleDeadlockResolution(msg.deadlockedTransactions);
                break;

            case NetworkMessageType::ABORT_TRANSACTION_SIGNAL:
                handleAbortTransactionSignal(msg.deadlockedTransactions);
                break;

            case NetworkMessageType::DISTRIBUTED_DETECTION_INIT:
                handleDistributedDetectionInit(msg.detectionZones, msg.detectionZoneLeaders);
                break;

            case NetworkMessageType::ZONE_DETECTION_REQUEST:
                handleZoneDetectionRequest(msg.centralNodeId, msg.zoneMembers);
                break;

            case NetworkMessageType::ZONE_WFG_REPORT:
                handleZoneWFGReport(msg.senderId, msg.wfgDataPairs);
                break;

            case NetworkMessageType::CENTRAL_WFG_REPORT_FROM_ZONE:
                handleCentralWFGReportFromZone(msg.senderId, msg.wfgDataPairs, msg.detectedCycles, msg.deadlockCount);
                break;

            case NetworkMessageType::PATH_PUSHING_PROBE:
                handlePathPushingProbe(msg);
                break;

            case NetworkMessageType::CLIENT_COLLECT_WFG_REQUEST:
                if (isCentralizedNode_) {
                    handleClientCollectWFGRequest(msg.senderId);
                }
                break;

            case NetworkMessageType::CLIENT_PRINT_DEADLOCK_REQUEST:
                if (isCentralizedNode_) {
                    handleClientPrintDeadlockRequest(msg.senderId);
                }
                break;

            case NetworkMessageType::CLIENT_RESOLVE_DEADLOCK_REQUEST:
                if (isCentralizedNode_) {
                    handleClientResolveDeadlockRequest(msg.victimTransId, msg.senderId);
                }
                break;

            case NetworkMessageType::DEADLOCK_REPORT_TO_CLIENT:
                break;

            case NetworkMessageType::UNKNOWN:
                break;

            default:
                break;
            }
        }
        catch (const std::runtime_error &e)
        {
            break;
        }
        catch (const std::exception &e)
        {
        }
    }
}
/**
 *  no use now
 */
// void DistributedDBNode::deadlockDetectionLoop() {
//     while (systemRunning) {
//         if (!systemRunning) break;
//     }
// }

void DistributedDBNode::centralizedDetectLoop() {
    while (systemRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(DEADLOCK_DETECTION_INTERVAL_MS));
        if (!systemRunning) break;
        if (isCentralizedNode_) {
            wfgReportsReceived_ = 0;
            aggregatedWfg_.clear();
            wfgReportsExpected_ = numNodes_;
            for (int i = 1; i <= numNodes_; ++i) {
                NetworkMessage requestMsg;
                requestMsg.type = NetworkMessageType::WFG_REPORT;
                requestMsg.senderId = nodeId_;
                requestMsg.receiverId = i;
                network_.sendMessage(requestMsg);
            }
        }
    }
}

void DistributedDBNode::distributedDetectCoordinatorLoop() {
    while (systemRunning) {
        if (!systemRunning) break;
        if (detectionZoneManager_.isZoneLeader()) {
            NodeId myZoneLeaderId = detectionZoneManager_.getMyZoneLeaderId();
            const std::vector<NodeId>& myZoneMembers = detectionZoneManager_.getMyDetectionZoneMembers();
            wfgReportsReceived_ = 0;
            aggregatedWfg_.clear();
            wfgReportsExpected_ = myZoneMembers.size();
            for (NodeId memberId : myZoneMembers) {
                NetworkMessage requestMsg;
                requestMsg.type = NetworkMessageType::ZONE_WFG_REPORT;
                requestMsg.senderId = nodeId_;
                requestMsg.receiverId = memberId;
                network_.sendMessage(requestMsg);
            }
        }
    }
}

void DistributedDBNode::pathPushingDetectionLoop() {
    while (systemRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(DEADLOCK_DETECTION_INTERVAL_MS));
        if (!systemRunning) break;
        initiatePathPushingProbes();
    }
}

void DistributedDBNode::pagSamplingLoop() {
    while (systemRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(PAG_SAMPLE_INTERVAL_MS));
        if (!systemRunning) break;
        if (DEADLOCK_DETECTION_MODE == MODE_HAWK && isCentralizedNode_) {
            pagResponsesReceived_ = 0;
            aggregatedPagEdges_.clear();
            pagResponsesExpected_ = numNodes_;
            for (int i = 1; i <= numNodes_; ++i) {
                NetworkMessage requestMsg;
                requestMsg.type = NetworkMessageType::PAG_REQUEST;
                requestMsg.senderId = nodeId_;
                requestMsg.receiverId = i;
                network_.sendMessage(requestMsg);
            }
        }
    }
}

// now we use broadcastTreeAdjustment in handlePAGResponse
// void DistributedDBNode::treeAdjustmentLoop() {
//     while (systemRunning) {
//         if (!systemRunning) break;
//     }
// }

/**
 * @brief Handles a WFG report message.
 *
 * In centralized deadlock detection mode, the central node receives WFG reports from other nodes.
 * It merges the WFG data from the report into its global aggregated WFG.
 *
 * @param reporterNodeId The ID of the node reporting the WFG.
 * @param wfgDataPairs The list of WFG data pairs contained in the report.
 */
void DistributedDBNode::handleWFGReport(NodeId reporterNodeId, const std::unordered_map<TransactionId, std::vector<TransactionId>> &wfgData)
{
    if (!isCentralizedNode_) return;

    std::unique_lock<std::mutex> lock(aggregatedWfgMutex_);
    mergeWFG(aggregatedWfg_, convertWFGToMessageFormat(wfgData));
    wfgReportsReceived_++;
    if (wfgReportsReceived_ >= wfgReportsExpected_)
    {
        checkAndResolveDeadlocks(aggregatedWfg_);
        aggregatedWfg_.clear();
        wfgReportsReceived_ = 0;
    }
}

void DistributedDBNode::handlePAGRequest(NodeId requesterNodeId)
{
    std::vector<WFDEdge> crossNodeEdges = lockTable_.collectCrossNodeWFDEdges();
    NetworkMessage responseMsg;
    responseMsg.type = NetworkMessageType::PAG_RESPONSE;
    responseMsg.senderId = nodeId_;
    responseMsg.receiverId = requesterNodeId;
    responseMsg.pagEdges = crossNodeEdges;
    network_.sendMessage(responseMsg);
}

void DistributedDBNode::handlePAGResponse(NodeId reporterNodeId, const std::vector<WFDEdge> &pagEdges)
{
    if (!isCentralizedNode_) return;

    std::unique_lock<std::mutex> lock(aggregatedPagEdgesMutex_);
    aggregatedPagEdges_.insert(aggregatedPagEdges_.end(), pagEdges.begin(), pagEdges.end());
    pagResponsesReceived_++;

    if (pagResponsesReceived_ >= pagResponsesExpected_) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTreeAdjustTime_).count();

        long long currentTotalDeadlocksFromZones = totalDeadlocksFromZones_.load();
        long long currentTotalDeadlocksFromCentral = totalDeadlocksFromCentral_.load();

        long long newDeadlocksFromZones = currentTotalDeadlocksFromZones - prevTotalDeadlocksFromZones_;
        long long newDeadlocksFromCentral = currentTotalDeadlocksFromCentral - prevTotalDeadlocksFromCentral_;

        prevTotalDeadlocksFromZones_ = currentTotalDeadlocksFromZones;
        prevTotalDeadlocksFromCentral_ = currentTotalDeadlocksFromCentral;
        lastTreeAdjustTime_ = currentTime;

        const int CHECK_INTERVAL_MS = 5000;
        const double CR_OVER_C_THRESHOLD = 1.0;

        bool shouldAdjustTree = false;

        if (duration >= CHECK_INTERVAL_MS) {
            if (newDeadlocksFromZones > 0) {
                double ratio = static_cast<double>(newDeadlocksFromCentral) / newDeadlocksFromZones;
                if (ratio > CR_OVER_C_THRESHOLD) {
                    shouldAdjustTree = true;
                    std::cout << "Node " << nodeId_ << ": CR/C ratio (" << ratio << ") > " << CR_OVER_C_THRESHOLD 
                              << ". Triggering Tree Adjustment.\n";
                } else {
                    std::cout << "Node " << nodeId_ << ": CR/C ratio (" << ratio << ") <= " << CR_OVER_C_THRESHOLD 
                              << ". Skipping Tree Adjustment.\n";
                }
            } else if (newDeadlocksFromCentral > 0) {
                shouldAdjustTree = true;
                 std::cout << "Node " << nodeId_ << ": Central detected deadlocks (" << newDeadlocksFromCentral 
                           << ") while zones detected none. Triggering Tree Adjustment.\n";
            } else {
                std::cout << "Node " << nodeId_ << ": No new deadlocks detected in last interval. Skipping Tree Adjustment.\n";
            }
        } else {
            std::cout << "Node " << nodeId_ << ": Not yet " << CHECK_INTERVAL_MS << "ms since last check. Skipping Tree Adjustment.\n";
        }

        if (shouldAdjustTree) {
            PAG fullPag = pagManager_.generatePAG(aggregatedPagEdges_);
            auto scc_result = pagManager_.greedySCCcut(fullPag, SCC_CUT_THRESHOLD);
            std::vector<std::vector<NodeId>> newDetectionZones = scc_result.first;
            std::vector<NodeId> newDetectionZoneLeaders = scc_result.second;
            network_.broadcastTreeAdjustment(nodeId_, newDetectionZones, newDetectionZoneLeaders);
        }

        aggregatedPagEdges_.clear();
        pagResponsesReceived_ = 0;
    }
}

void DistributedDBNode::checkAndResolveDeadlocks(const std::unordered_map<TransactionId, std::vector<TransactionId>> &graph)
{
    std::unordered_map<TransactionId, std::vector<TransactionId>> prunedGraph;
    std::unordered_set<TransactionId> activeTxns = transactionManager_.getActiveTransactions();
    for (const auto& pair : graph) {
        if (activeTxns.count(pair.first)) {
            for (TransactionId target : pair.second) {
                if (activeTxns.count(target)) {
                    prunedGraph[pair.first].push_back(target);
                }
            }
        }
    }

    if (prunedGraph.empty()) return;

    auto result = deadlockDetector_->findCycles(prunedGraph);
    std::vector<std::vector<TransactionId>> detectedCycles = result.first;
    std::unordered_map<TransactionId, int> transactionFrequencies = result.second;

    if (!detectedCycles.empty())
    {
        for (const auto &cycle : detectedCycles)
        {
            TransactionId victimId = selectVictim(cycle, transactionFrequencies);
            NodeId victimHomeNode = transactionManager_.getTransactionHomeNode(victimId);
            if (victimHomeNode != 0) {
                NetworkMessage abortMsg;
                abortMsg.type = NetworkMessageType::ABORT_TRANSACTION_SIGNAL;
                abortMsg.senderId = nodeId_;
                abortMsg.receiverId = victimHomeNode;
                abortMsg.deadlockedTransactions.push_back(victimId);
                network_.sendMessage(abortMsg);
            }
        }
    }

    if (isCentralizedNode_) {
        NetworkMessage reportToClientMsg;
        reportToClientMsg.type = NetworkMessageType::DEADLOCK_REPORT_TO_CLIENT;
        reportToClientMsg.senderId = nodeId_;
        reportToClientMsg.receiverId = 0;
        reportToClientMsg.detectedCycles = detectedCycles;
        reportToClientMsg.deadlockCount = detectedCycles.size();
        network_.sendMessage(reportToClientMsg);
    }
}

void DistributedDBNode::checkAndResolveDeadlocksForZone(NodeId zoneLeaderId, const std::unordered_map<TransactionId, std::vector<TransactionId>> &graph)
{
    std::unordered_map<TransactionId, std::vector<TransactionId>> prunedGraph;
    std::unordered_set<TransactionId> activeTxns = transactionManager_.getActiveTransactions();
    for (const auto& pair : graph) {
        if (activeTxns.count(pair.first)) {
            for (TransactionId target : pair.second) {
                if (activeTxns.count(target)) {
                    prunedGraph[pair.first].push_back(target);
                }
            }
        }
    }

    if (prunedGraph.empty()) {
        if (DEADLOCK_DETECTION_MODE == MODE_HAWK && zoneLeaderId != CENTRALIZED_NODE_ID) {
            NetworkMessage reportMsg;
            reportMsg.type = NetworkMessageType::CENTRAL_WFG_REPORT_FROM_ZONE;
            reportMsg.senderId = nodeId_;
            reportMsg.receiverId = CENTRALIZED_NODE_ID;
            network_.sendMessage(reportMsg);
        }
        return;
    }

    auto result = deadlockDetector_->findCycles(prunedGraph);
    std::vector<std::vector<TransactionId>> detectedCycles = result.first;

    if (!detectedCycles.empty()) {
        for (const auto &cycle : detectedCycles) {
            TransactionId victimId = selectVictim(cycle, result.second);
            NodeId victimHomeNode = transactionManager_.getTransactionHomeNode(victimId);
            if (victimHomeNode != 0) {
                NetworkMessage abortMsg;
                abortMsg.type = NetworkMessageType::ABORT_TRANSACTION_SIGNAL;
                abortMsg.senderId = nodeId_;
                abortMsg.receiverId = victimHomeNode;
                abortMsg.deadlockedTransactions.push_back(victimId);
                network_.sendMessage(abortMsg);
            }
        }
    }

    if (DEADLOCK_DETECTION_MODE == MODE_HAWK && zoneLeaderId != CENTRALIZED_NODE_ID) {
        NetworkMessage reportMsg;
        reportMsg.type = NetworkMessageType::CENTRAL_WFG_REPORT_FROM_ZONE;
        reportMsg.senderId = nodeId_;
        reportMsg.receiverId = CENTRALIZED_NODE_ID;
        reportMsg.wfgDataPairs = convertWFGToMessageFormat(prunedGraph);
        reportMsg.detectedCycles = detectedCycles;
        reportMsg.deadlockCount = detectedCycles.size();
        network_.sendMessage(reportMsg);
    }
}

TransactionId DistributedDBNode::selectVictim(const std::vector<TransactionId> &cycle,
                                              const std::unordered_map<TransactionId, int> &transactionFrequencies)
{
    std::vector<std::pair<TransactionId, int>> candidates;
    for (TransactionId tid : cycle)
    {
        candidates.push_back({tid, transactionFrequencies.at(tid)});
    }
    std::sort(candidates.begin(), candidates.end(), DeadlockDetector::compareTransactionPriority);
    return candidates[0].first;
}

void DistributedDBNode::handleDeadlockResolution(const std::vector<TransactionId> &transIdsToAbort)
{
    for (TransactionId tid : transIdsToAbort)
    {
        transactionManager_.abortTransaction(tid);
    }
}

void DistributedDBNode::handleAbortTransactionSignal(const std::vector<TransactionId> &transIdsToAbort)
{
    for (TransactionId tid : transIdsToAbort)
    {
        transactionManager_.abortTransaction(tid);
    }
}

void DistributedDBNode::handleDistributedDetectionInit(const std::vector<std::vector<NodeId>>& detectionZones, const std::vector<NodeId>& detectionZoneLeaders) {
    detectionZoneManager_.updateDetectionZones(detectionZones, detectionZoneLeaders);
}

void DistributedDBNode::handleZoneDetectionRequest(NodeId centralNodeId, const std::vector<NodeId>& zoneMembers) {
    std::unordered_set<TransactionId> activeTxns = transactionManager_.getActiveTransactions();
    std::unordered_map<TransactionId, std::vector<TransactionId>> lwfg = lockTable_.buildAndPruneLocalWaitForGraph(activeTxns);
    NetworkMessage reportMsg;
    reportMsg.type = NetworkMessageType::ZONE_WFG_REPORT;
    reportMsg.senderId = nodeId_;
    reportMsg.receiverId = centralNodeId;
    reportMsg.wfgDataPairs = convertWFGToMessageFormat(lwfg);
    network_.sendMessage(reportMsg);
}

void DistributedDBNode::handleZoneWFGReport(NodeId reporterNodeId, const std::vector<std::pair<TransactionId, std::vector<TransactionId>>> &wfgDataPairs) {
    if (!detectionZoneManager_.isZoneLeader()) return;
    std::unique_lock<std::mutex> lock(aggregatedWfgMutex_);
    mergeWFG(aggregatedWfg_, wfgDataPairs);
    wfgReportsReceived_++;
    if (wfgReportsReceived_ >= wfgReportsExpected_) {
        checkAndResolveDeadlocksForZone(nodeId_, aggregatedWfg_);
        aggregatedWfg_.clear();
        wfgReportsReceived_ = 0;
    }
}

void DistributedDBNode::handleCentralWFGReportFromZone(NodeId zoneLeaderId, 
    const std::vector<std::pair<TransactionId, std::vector<TransactionId>>> &wfgDataPairs, 
    const std::vector<std::vector<TransactionId>>& detectedCycles, 
    int reportedDeadlockCount) {
if (!isCentralizedNode_) return;
std::unique_lock<std::mutex> lock(centralAggregatedWfgMutex_);
mergeWFG(centralAggregatedWfg_, wfgDataPairs);
centralWfgReportsReceived_++;

totalDeadlocksFromZones_ += reportedDeadlockCount; 
for (const auto& cycle : detectedCycles) {
centralDetectedCycles_.push_back(cycle);
}

if (centralWfgReportsReceived_ >= numNodes_) { 
std::pair<std::vector<std::vector<TransactionId>>, std::unordered_map<TransactionId, int>> global_detection_result = 
deadlockDetector_->findCycles(centralAggregatedWfg_);

std::vector<std::vector<TransactionId>> globalDetectedCycles = global_detection_result.first;

totalDeadlocksFromCentral_ += globalDetectedCycles.size(); 

centralDeadlockCount_ += globalDetectedCycles.size(); 
for (const auto& cycle : globalDetectedCycles) {
centralDetectedCycles_.push_back(cycle);
}

NetworkMessage reportToClientMsg;
reportToClientMsg.type = NetworkMessageType::DEADLOCK_REPORT_TO_CLIENT;
reportToClientMsg.senderId = nodeId_;
reportToClientMsg.receiverId = 0; 
reportToClientMsg.detectedCycles = centralDetectedCycles_; 
reportToClientMsg.deadlockCount = centralDeadlockCount_;     
network_.sendMessage(reportToClientMsg);

centralAggregatedWfg_.clear();
centralWfgReportsReceived_ = 0;
centralDeadlockCount_ = 0; 
centralDetectedCycles_.clear();
}
}

void DistributedDBNode::handlePathPushingProbe(const NetworkMessage& msg) {
    if (msg.path.empty()) return;
    TransactionId lastTransInPath = msg.path.back();
    std::shared_ptr<Transaction> trans = transactionManager_.getTransaction(lastTransInPath);
    if (!trans || trans->status != TransactionStatus::BLOCKED) return;

    ResourceId waitingForRes = trans->waitingForResourceId;
    if (waitingForRes == 0) return;

    auto holders = resourceManager_.getResourceHolders(waitingForRes);
    if (holders.empty()) return;

    TransactionId blockingTransId = holders.begin()->first;
    NodeId blockingTransHomeNode = transactionManager_.getTransactionHomeNode(blockingTransId);
    std::vector<TransactionId> newPath = msg.path;
    newPath.push_back(blockingTransId);

    if (std::find(msg.path.begin(), msg.path.end(), blockingTransId) != msg.path.end()) {
        TransactionId victimId = selectVictim(newPath, {});
        NodeId victimHomeNode = transactionManager_.getTransactionHomeNode(victimId);
        if (victimHomeNode != 0) {
            NetworkMessage abortMsg;
            abortMsg.type = NetworkMessageType::ABORT_TRANSACTION_SIGNAL;
            abortMsg.senderId = nodeId_;
            abortMsg.receiverId = victimHomeNode;
            abortMsg.deadlockedTransactions.push_back(victimId);
            network_.sendMessage(abortMsg);
        }
        if (isCentralizedNode_) {
            NetworkMessage reportToClientMsg;
            reportToClientMsg.type = NetworkMessageType::DEADLOCK_REPORT_TO_CLIENT;
            reportToClientMsg.senderId = nodeId_;
            reportToClientMsg.receiverId = 0;
            reportToClientMsg.detectedCycles.push_back(newPath);
            reportToClientMsg.deadlockCount = 1;
            network_.sendMessage(reportToClientMsg);
        }
    }
    else if (blockingTransHomeNode != 0) {
        NetworkMessage newProbeMsg;
        newProbeMsg.type = NetworkMessageType::PATH_PUSHING_PROBE;
        newProbeMsg.senderId = nodeId_;
        newProbeMsg.receiverId = blockingTransHomeNode;
        newProbeMsg.path = newPath;
        network_.sendMessage(newProbeMsg);
    }
}

void DistributedDBNode::initiatePathPushingProbes() {
    std::unordered_set<TransactionId> activeTxns = transactionManager_.getActiveTransactions();
    for (TransactionId transId : activeTxns) {
        std::shared_ptr<Transaction> trans = transactionManager_.getTransaction(transId);
        if (trans && trans->status == TransactionStatus::BLOCKED) {
            NetworkMessage probeMsg;
            probeMsg.type = NetworkMessageType::PATH_PUSHING_PROBE;
            probeMsg.senderId = nodeId_;
            probeMsg.receiverId = nodeId_;
            probeMsg.path.push_back(transId);
            network_.getIncomingQueue()->push(probeMsg);
        }
    }
}

void DistributedDBNode::mergeWFG(std::unordered_map<TransactionId, std::vector<TransactionId>>& targetWfg,
                                const std::vector<std::pair<TransactionId, std::vector<TransactionId>>>& sourceData) {
    for (const auto& pair : sourceData) {
        for (TransactionId target : pair.second) {
            targetWfg[pair.first].push_back(target);
        }
    }
}

std::vector<std::pair<TransactionId, std::vector<TransactionId>>>
DistributedDBNode::convertWFGToMessageFormat(const std::unordered_map<TransactionId, std::vector<TransactionId>>& wfg) {
    std::vector<std::pair<TransactionId, std::vector<TransactionId>>> messageFormat;
    for (const auto& pair : wfg) {
        messageFormat.push_back({pair.first, pair.second});
    }
    return messageFormat;
}

void DistributedDBNode::handleClientCollectWFGRequest(NodeId clientId) {
    if (!isCentralizedNode_) return;
    std::unordered_map<TransactionId, std::vector<TransactionId>> currentAggregatedWfg;
    std::unordered_set<TransactionId> allActiveTxns;
    for (int i = 1; i <= NUM_NODES; ++i) {}
}

void DistributedDBNode::handleClientPrintDeadlockRequest(NodeId clientId) {
    if (!isCentralizedNode_) return;
    NetworkMessage reportToClientMsg;
    reportToClientMsg.type = NetworkMessageType::DEADLOCK_REPORT_TO_CLIENT;
    reportToClientMsg.senderId = nodeId_;
    reportToClientMsg.receiverId = clientId;
    reportToClientMsg.detectedCycles = centralDetectedCycles_;
    reportToClientMsg.deadlockCount = centralDeadlockCount_;
    network_.sendMessage(reportToClientMsg);
}

void DistributedDBNode::handleClientResolveDeadlockRequest(TransactionId victimTransId, NodeId clientId) {
    if (!isCentralizedNode_) return;
    NodeId victimHomeNode = transactionManager_.getTransactionHomeNode(victimTransId);
    if (victimHomeNode != 0) {
        NetworkMessage abortMsg;
        abortMsg.type = NetworkMessageType::ABORT_TRANSACTION_SIGNAL;
        abortMsg.senderId = nodeId_;
        abortMsg.receiverId = victimHomeNode;
        abortMsg.deadlockedTransactions.push_back(victimTransId);
        network_.sendMessage(abortMsg);
    }
    NetworkMessage confirmationMsg;
    confirmationMsg.type = NetworkMessageType::DEADLOCK_REPORT_TO_CLIENT;
    confirmationMsg.senderId = nodeId_;
    confirmationMsg.receiverId = clientId;
    confirmationMsg.deadlockCount = 0;
    network_.sendMessage(confirmationMsg);
}