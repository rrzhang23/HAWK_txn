#ifndef HAWK_TRANSACTION_MANAGER_H
#define HAWK_TRANSACTION_MANAGER_H

#include "commons.h"
#include "ResourceManager.h"
#include "SafeQueue.h"
#include "Transaction.h"
#include "RandomGenerators.h"
#include "tpcc_data_generator.h"

#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <functional>
#include <unordered_set>

class Network;

class TransactionManager
{
public:
    friend class DistributedDBNode;
    friend class LockTable;

    TransactionManager(NodeId nodeId, ResourceManager &resourceManager,
                       std::shared_ptr<SafeQueue<NetworkMessage>> incomingNetworkQueue,
                       std::function<void(const NetworkMessage &)> sendNetworkMessage,
                       TPCCRandom& rng);

    std::shared_ptr<Transaction> beginTransaction();

    std::shared_ptr<Transaction> beginControlledTransaction(const std::vector<SQLStatement>& statements);

    bool tryExecuteNextSQLStatement(TransactionId transId);

    void handleSQLResponse(TransactionId transId, bool granted, ResourceId resId);

    void abortTransaction(TransactionId transId);

    void commitTransaction(TransactionId transId);

    std::unordered_set<TransactionId> getActiveTransactions();

    std::unordered_map<ResourceId, LockMode> getTransactionLocks(TransactionId transId);

    ResourceId getTransactionWaitingFor(TransactionId transId);

    SQLStatement generateControlledSQLStatement(TransactionId transId, NodeId homeNodeId,
                                                const std::vector<ResourceId> &resources,
                                                LockMode mode);

    std::vector<long long> getCompletedTransactionLatencies();

    void addTPCCTransaction(std::shared_ptr<Transaction> tpccTrans);

    TransactionId getNextTransactionId();

    std::shared_ptr<Transaction> getTransaction(TransactionId transId);

    NodeId getTransactionHomeNode(TransactionId transId);

private:
    NodeId nodeId;
    ResourceManager &resourceManager;
    std::shared_ptr<SafeQueue<NetworkMessage>> incomingNetworkQueue;
    std::function<void(const NetworkMessage &)> sendNetworkMessage;

    TransactionId nextTransactionId = 1;

    std::unordered_map<TransactionId, std::shared_ptr<Transaction>> activeTransactions;
    std::mutex activeTransactionsMutex;

    SafeQueue<long long> completedTransactionLatencies_;

    void notifyTransactionToRetryAcquire(TransactionId transId, ResourceId resId);

    std::vector<SQLStatement> generateRandomSQLStatements(TransactionId transId, NodeId homeNodeId);
    TPCCRandom& rng_;
};

#endif // HAWK_TRANSACTION_MANAGER_H