#include "LockTable.h"
#include <iostream>
#include <algorithm>

LockTable::LockTable(NodeId nodeId, ResourceManager &resourceManager,
                     TransactionManager &transactionManager)
    : nodeId(nodeId), resourceManager(resourceManager),
      transactionManager(transactionManager) {}

std::unordered_map<TransactionId, std::vector<TransactionId>>
LockTable::buildLocalWaitForGraph()
{
    std::unordered_map<TransactionId, std::vector<TransactionId>> lwfg;
    for (ResourceId resId : resourceManager.getLocalResources())
    {
        auto currentHolders = resourceManager.getResourceHolders(resId);
        if (currentHolders.empty())
            continue;

        std::queue<TransactionId> waitingQueue = resourceManager.getResourceWaitingQueue(resId);
        if (waitingQueue.empty())
            continue;

        TransactionId waitingTransId = waitingQueue.front();

        ResourceId transWaitingFor = transactionManager.getTransactionWaitingFor(waitingTransId);
        if (transWaitingFor != resId)
        {
            continue;
        }

        for (const auto &holderPair : currentHolders)
        {
            if (holderPair.first != waitingTransId)
            {
                lwfg[waitingTransId].push_back(holderPair.first);
            }
        }
    }
    return lwfg;
}

std::unordered_map<TransactionId, std::vector<TransactionId>>
LockTable::buildAndPruneLocalWaitForGraph(const std::unordered_set<TransactionId>& active_transaction_ids)
{
    std::unordered_map<TransactionId, std::vector<TransactionId>> lwfg;
    for (ResourceId resId : resourceManager.getLocalResources())
    {
        auto currentHolders = resourceManager.getResourceHolders(resId);
        if (currentHolders.empty())
            continue;

        std::queue<TransactionId> waitingQueue = resourceManager.getResourceWaitingQueue(resId);
        if (waitingQueue.empty())
            continue;

        TransactionId waitingTransId = waitingQueue.front();

        ResourceId transWaitingFor = transactionManager.getTransactionWaitingFor(waitingTransId);
        if (transWaitingFor != resId || active_transaction_ids.find(waitingTransId) == active_transaction_ids.end())
        {
            continue;
        }

        for (const auto &holderPair : currentHolders)
        {
            if (holderPair.first != waitingTransId && active_transaction_ids.find(holderPair.first) != active_transaction_ids.end())
            {
                lwfg[waitingTransId].push_back(holderPair.first);
            }
        }
    }
    return lwfg;
}

void LockTable::printLockTableState()
{
    std::cout << "--- Lock Table State (Node " << nodeId << ") ---\n";
    bool hasLocks = false;
    for (ResourceId resId : resourceManager.getLocalResources())
    {
        auto holders = resourceManager.getResourceHolders(resId);
        if (!holders.empty())
        {
            hasLocks = true;
            std::cout << "  R" << resId << " held by: ";
            for (const auto &pair : holders)
            {
                std::cout << "T" << pair.first << "(" << (pair.second == LockMode::EXCLUSIVE ? "EX" : "SH") << ") ";
            }
            std::cout << "\n";
        }
        std::queue<TransactionId> waitingQueue = resourceManager.getResourceWaitingQueue(resId);
        if (!waitingQueue.empty())
        {
            hasLocks = true;
            std::cout << "  R" << resId << " waiting queue: ";
            std::queue<TransactionId> tempQueue = waitingQueue;
            while (!tempQueue.empty())
            {
                std::cout << "T" << tempQueue.front() << " ";
                tempQueue.pop();
            }
            std::cout << "\n";
        }
    }
    if (!hasLocks)
    {
        std::cout << "  No locks or waiting transactions.\n";
    }
    std::cout << "-----------------------------------\n";
}

std::vector<WFDEdge> LockTable::collectCrossNodeWFDEdges()
{
    std::vector<WFDEdge> crossNodeEdges;
    for (ResourceId resId : resourceManager.getLocalResources())
    {
        auto currentHolders = resourceManager.getResourceHolders(resId);
        if (currentHolders.empty())
            continue;

        std::queue<TransactionId> waitingQueue = resourceManager.getResourceWaitingQueue(resId);
        if (waitingQueue.empty())
            continue;

        TransactionId waitingTransId = waitingQueue.front();
        ResourceId transWaitingFor = transactionManager.getTransactionWaitingFor(waitingTransId);
        if (transWaitingFor != resId)
        {
            continue;
        }

        for (const auto &holderPair : currentHolders)
        {
            NodeId waitingNode = transactionManager.activeTransactions.at(waitingTransId)->homeNodeId;
            NodeId heldNode = transactionManager.activeTransactions.at(holderPair.first)->homeNodeId;

            if (waitingNode != heldNode)
            {
                crossNodeEdges.push_back({waitingTransId, holderPair.first, waitingNode, heldNode});
            }
        }
    }
    return crossNodeEdges;
}

bool LockTable::acquireLock(TransactionId transId, ResourceId resId, LockMode mode) {
    return resourceManager.acquireLock(transId, resId, mode);
}

void LockTable::releaseAllLocks(TransactionId transId) {
    resourceManager.releaseAllLocks(transId);
}