#include "TransactionManager.h"
#include "tpcc_data_generator.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <random>

TransactionManager::TransactionManager(NodeId nodeId, ResourceManager &resourceManager,
                                       std::shared_ptr<SafeQueue<NetworkMessage>> incomingNetworkQueue,
                                       std::function<void(const NetworkMessage &)> sendNetworkMessage,
                                       TPCCRandom& rng)
    : nodeId(nodeId), resourceManager(resourceManager),
      incomingNetworkQueue(incomingNetworkQueue),
      sendNetworkMessage(sendNetworkMessage),
      rng_(rng)
{
    resourceManager.notifyTransactionToRetryAcquire =
        std::bind(&TransactionManager::notifyTransactionToRetryAcquire, this, std::placeholders::_1, std::placeholders::_2);
}

std::shared_ptr<Transaction> TransactionManager::beginTransaction()
{
    std::shared_ptr<Transaction> newTrans = std::make_shared<Transaction>();
    newTrans->id = getNextTransactionId();
    newTrans->homeNodeId = nodeId;
    newTrans->startTime = std::chrono::high_resolution_clock::now();

    int numSqls = RandomGenerators::getExponentialInt(SQL_COUNT_LAMBDA, MIN_SQLS_PER_TRANSACTION, MAX_SQLS_PER_TRANSACTION);
    newTrans->statements = generateRandomSQLStatements(newTrans->id, newTrans->homeNodeId);

    std::unique_lock<std::mutex> lock(activeTransactionsMutex);
    activeTransactions[newTrans->id] = newTrans;
    return newTrans;
}

std::shared_ptr<Transaction> TransactionManager::beginControlledTransaction(const std::vector<SQLStatement>& statements)
{
    std::shared_ptr<Transaction> newTrans = std::make_shared<Transaction>();
    newTrans->id = getNextTransactionId();
    newTrans->homeNodeId = nodeId;
    newTrans->startTime = std::chrono::high_resolution_clock::now();
    newTrans->statements = statements;

    std::unique_lock<std::mutex> lock(activeTransactionsMutex);
    activeTransactions[newTrans->id] = newTrans;
    return newTrans;
}

bool TransactionManager::tryExecuteNextSQLStatement(TransactionId transId)
{
    std::shared_ptr<Transaction> trans;
    {
        std::unique_lock<std::mutex> lock(activeTransactionsMutex);
        auto it = activeTransactions.find(transId);
        if (it == activeTransactions.end())
        {
            return false;
        }
        trans = it->second;
    }

    if (trans->status == TransactionStatus::BLOCKED)
    {
        return false;
    }

    if (trans->currentSQLIndex >= trans->statements.size())
    {
        commitTransaction(transId);
        return true;
    }

    SQLStatement &currentSQL = trans->statements[trans->currentSQLIndex];

    ResourceId resId = currentSQL.resources[0];
    NodeId ownerNodeId = getOwnerNodeId(resId);

    if (ownerNodeId == nodeId)
    {
        if (resourceManager.acquireLock(transId, resId, currentSQL.lockMode))
        {
            trans->currentSQLIndex++;
            trans->acquiredLocks[resId] = currentSQL.lockMode;
            return true;
        }
        else
        {
            trans->status = TransactionStatus::BLOCKED;
            trans->waitingForResourceId = resId;
            return false;
        }
    }
    else
    {
        NetworkMessage requestMsg;
        requestMsg.type = NetworkMessageType::LOCK_REQUEST;
        requestMsg.senderId = nodeId;
        requestMsg.receiverId = ownerNodeId;
        requestMsg.transId = transId;
        requestMsg.resId = resId;
        requestMsg.mode = currentSQL.lockMode;

        {
            std::unique_lock<std::mutex> lock(trans->remoteRequestMutex);
            trans->remoteRequestPending = true;
            trans->waitingForResourceId = resId;
        }

        sendNetworkMessage(requestMsg);

        trans->status = TransactionStatus::BLOCKED;
        return false;
    }
}

void TransactionManager::handleSQLResponse(TransactionId transId, bool granted, ResourceId resId)
{
    std::shared_ptr<Transaction> trans;
    {
        std::unique_lock<std::mutex> lock(activeTransactionsMutex);
        auto it = activeTransactions.find(transId);
        if (it == activeTransactions.end())
        {
            std::cerr << "Node " << nodeId << ": Received SQL response for unknown transaction " << transId << ". Ignoring.\n";
            return;
        }
        trans = it->second;
    }

    {
        std::unique_lock<std::mutex> lock(trans->remoteRequestMutex);
        trans->remoteRequestPending = false;
        trans->remoteRequestSuccess = granted;
        trans->waitingForResourceId = 0;
        trans->remoteRequestCv.notify_one();
    }

    if (granted)
    {
        trans->currentSQLIndex++;
        trans->acquiredLocks[resId] = trans->statements[trans->currentSQLIndex - 1].lockMode;
        trans->status = TransactionStatus::RUNNING;
        tryExecuteNextSQLStatement(transId);
    }
    else
    {
        trans->status = TransactionStatus::BLOCKED;
    }
}

void TransactionManager::abortTransaction(TransactionId transId)
{
    std::shared_ptr<Transaction> trans;
    {
        std::unique_lock<std::mutex> lock(activeTransactionsMutex);
        auto it = activeTransactions.find(transId);
        if (it == activeTransactions.end())
        {
            std::cerr << "Node " << nodeId << ": Attempted to abort unknown transaction " << transId << ". Ignoring.\n";
            return;
        }
        trans = it->second;
    }

    resourceManager.releaseAllLocks(transId);
    trans->status = TransactionStatus::ABORTED;

    auto endTime = std::chrono::high_resolution_clock::now();
    long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - trans->startTime).count();
    completedTransactionLatencies_.push(duration);

    std::unique_lock<std::mutex> lock(activeTransactionsMutex);
    activeTransactions.erase(transId);
}

void TransactionManager::commitTransaction(TransactionId transId)
{
    std::shared_ptr<Transaction> trans;
    {
        std::unique_lock<std::mutex> lock(activeTransactionsMutex);
        auto it = activeTransactions.find(transId);
        if (it == activeTransactions.end())
        {
            std::cerr << "Node " << nodeId << ": Attempted to commit unknown transaction " << transId << ". Ignoring.\n";
            return;
        }
        trans = it->second;
    }

    resourceManager.releaseAllLocks(transId);
    trans->status = TransactionStatus::COMMITTED;

    auto endTime = std::chrono::high_resolution_clock::now();
    long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - trans->startTime).count();
    completedTransactionLatencies_.push(duration);

    std::unique_lock<std::mutex> lock(activeTransactionsMutex);
    activeTransactions.erase(transId);
}

std::unordered_set<TransactionId> TransactionManager::getActiveTransactions()
{
    std::unique_lock<std::mutex> lock(activeTransactionsMutex);
    std::unordered_set<TransactionId> ids;
    for (const auto &pair : activeTransactions)
    {
        ids.insert(pair.first);
    }
    return ids;
}

std::unordered_map<ResourceId, LockMode> TransactionManager::getTransactionLocks(TransactionId transId)
{
    std::unique_lock<std::mutex> lock(activeTransactionsMutex);
    auto it = activeTransactions.find(transId);
    if (it != activeTransactions.end())
    {
        return it->second->acquiredLocks;
    }
    return {};
}

ResourceId TransactionManager::getTransactionWaitingFor(TransactionId transId)
{
    std::unique_lock<std::mutex> lock(activeTransactionsMutex);
    auto it = activeTransactions.find(transId);
    if (it != activeTransactions.end())
    {
        return it->second->waitingForResourceId;
    }
    return 0;
}

SQLStatement TransactionManager::generateControlledSQLStatement(TransactionId transId, NodeId homeNodeId,
                                                const std::vector<ResourceId> &resources,
                                                LockMode mode)
{
    SQLStatement sql;
    sql.transId = transId;
    sql.homeNodeId = homeNodeId;
    sql.resources = resources;
    sql.lockMode = mode;
    return sql;
}

std::vector<long long> TransactionManager::getCompletedTransactionLatencies() {
    return completedTransactionLatencies_.drain();
}

void TransactionManager::addTPCCTransaction(std::shared_ptr<Transaction> tpccTrans) {
    std::unique_lock<std::mutex> lock(activeTransactionsMutex);
    activeTransactions[tpccTrans->id] = tpccTrans;
}

TransactionId TransactionManager::getNextTransactionId() {
    return nextTransactionId++;
}

std::shared_ptr<Transaction> TransactionManager::getTransaction(TransactionId transId) {
    std::unique_lock<std::mutex> lock(activeTransactionsMutex);
    auto it = activeTransactions.find(transId);
    if (it != activeTransactions.end()) {
        return it->second;
    }
    return nullptr;
}

NodeId TransactionManager::getTransactionHomeNode(TransactionId transId) {
    std::unique_lock<std::mutex> lock(activeTransactionsMutex);
    auto it = activeTransactions.find(transId);
    if (it != activeTransactions.end()) {
        return it->second->homeNodeId;
    }
    return 0;
}

std::vector<SQLStatement> TransactionManager::generateRandomSQLStatements(TransactionId transId, NodeId homeNodeId)
{
    std::vector<SQLStatement> statements;
    int numSqls = RandomGenerators::getExponentialInt(SQL_COUNT_LAMBDA, MIN_SQLS_PER_TRANSACTION, MAX_SQLS_PER_TRANSACTION);
    for (int i = 0; i < numSqls; ++i)
    {
        SQLStatement sql;
        sql.transId = transId;
        sql.homeNodeId = homeNodeId;
        sql.resources.push_back(RandomGenerators::getRandomInt(1, TOTAL_RESOURCES));
        sql.lockMode = (RandomGenerators::getRandomDouble(0.0, 1.0) < EXCLUSIVE_LOCK_PROBABILITY) ? LockMode::EXCLUSIVE : LockMode::SHARED;
        statements.push_back(sql);
    }
    return statements;
}