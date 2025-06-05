#ifndef HAWK_LOCK_TABLE_H
#define HAWK_LOCK_TABLE_H

#include "commons.h"
#include "ResourceManager.h"
#include "TransactionManager.h"

#include <unordered_map>
#include <vector>
#include <queue>
#include <mutex>
#include <unordered_set>

class LockTable
{
public:
    LockTable(NodeId nodeId, ResourceManager &resourceManager,
              TransactionManager &transactionManager);

    std::unordered_map<TransactionId, std::vector<TransactionId>>
    buildLocalWaitForGraph();

    std::unordered_map<TransactionId, std::vector<TransactionId>>
    buildAndPruneLocalWaitForGraph(const std::unordered_set<TransactionId>& active_transaction_ids);

    void printLockTableState();

    std::vector<WFDEdge> collectCrossNodeWFDEdges();

    bool acquireLock(TransactionId transId, ResourceId resId, LockMode mode);
    void releaseAllLocks(TransactionId transId);

private:
    NodeId nodeId;
    ResourceManager &resourceManager;
    TransactionManager &transactionManager;
    std::mutex wfgMutex;
};

#endif