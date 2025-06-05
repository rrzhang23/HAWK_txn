#include "ResourceManager.h"
#include <iostream>
#include <algorithm>

ResourceManager::ResourceManager(NodeId nodeId) : nodeId_(nodeId)
{
    notifyTransactionToRetryAcquire = nullptr;
}

bool ResourceManager::acquireLock(TransactionId transId, ResourceId resId, LockMode mode)
{
    if (getOwnerNodeId(resId) != nodeId_)
    {
        return false;
    }

    std::unique_lock<std::mutex> holders_lock(resourceHoldersMutex_);
    std::unique_lock<std::mutex> queues_lock(resourceWaitingQueuesMutex_);

    if (checkConflict(resId, mode))
    {
        resourceWaitingQueues_[resId].push(transId);
        std::cout << "Node " << nodeId_ << ": Trans " << transId << " BLOCKED on R" << resId << " (Mode: " << (mode == LockMode::EXCLUSIVE ? "EX" : "SH") << ").\n";
        return false;
    }
    else
    {
        resourceHolders_[resId][transId] = mode;
        std::cout << "Node " << nodeId_ << ": Trans " << transId << " acquired R" << resId << " (Mode: " << (mode == LockMode::EXCLUSIVE ? "EX" : "SH") << ").\n";
        return true;
    }
}

void ResourceManager::releaseLock(TransactionId transId, ResourceId resId)
{
    if (getOwnerNodeId(resId) != nodeId_)
    {
        std::cerr << "Node " << nodeId_ << ": Attempted to release remote resource R" << resId << ".\n";
        return;
    }

    std::unique_lock<std::mutex> holders_lock(resourceHoldersMutex_);
    std::unique_lock<std::mutex> queues_lock(resourceWaitingQueuesMutex_);

    if (resourceHolders_.count(resId) && resourceHolders_[resId].count(transId))
    {
        resourceHolders_[resId].erase(transId);
        std::cout << "Node " << nodeId_ << ": Trans " << transId << " released R" << resId << ".\n";

        if (resourceWaitingQueues_.count(resId) && !resourceWaitingQueues_[resId].empty())
        {
            TransactionId waitingTransId = resourceWaitingQueues_[resId].front();
            if (notifyTransactionToRetryAcquire) {
                notifyTransactionToRetryAcquire(waitingTransId, resId);
            }
        }

        if (resourceHolders_[resId].empty()) {
            resourceHolders_.erase(resId);
        }
    }
    else
    {
        std::cerr << "Node " << nodeId_ << ": Trans " << transId << " does not hold lock on R" << resId << " to release.\n";
    }
}

void ResourceManager::releaseAllLocks(TransactionId transId)
{
    std::unique_lock<std::mutex> holders_lock(resourceHoldersMutex_);
    std::unique_lock<std::mutex> queues_lock(resourceWaitingQueuesMutex_);

    std::vector<ResourceId> releasedResources;

    for (auto it_res = resourceHolders_.begin(); it_res != resourceHolders_.end(); )
    {
        ResourceId resId = it_res->first;
        auto& holders = it_res->second;

        if (holders.count(transId))
        {
            holders.erase(transId);
            std::cout << "Node " << nodeId_ << ": Trans " << transId << " released R" << resId << " (part of all locks release).\n";
            releasedResources.push_back(resId);

            if (holders.empty())
            {
                it_res = resourceHolders_.erase(it_res);
            }
            else
            {
                ++it_res;
            }
        }
        else
        {
            ++it_res;
        }
    }

    for (ResourceId resId : releasedResources)
    {
        if (resourceWaitingQueues_.count(resId) && !resourceWaitingQueues_[resId].empty())
        {
            TransactionId waitingTransId = resourceWaitingQueues_[resId].front();
            if (notifyTransactionToRetryAcquire) {
                notifyTransactionToRetryAcquire(waitingTransId, resId);
            }
        }
    }
}

std::unordered_map<TransactionId, LockMode> ResourceManager::getResourceHolders(ResourceId resId)
{
    std::unique_lock<std::mutex> lock(resourceHoldersMutex_);
    if (resourceHolders_.count(resId))
    {
        return resourceHolders_[resId];
    }
    return {};
}

std::queue<TransactionId> ResourceManager::getResourceWaitingQueue(ResourceId resId)
{
    std::unique_lock<std::mutex> lock(resourceWaitingQueuesMutex_);
    if (resourceWaitingQueues_.count(resId))
    {
        return resourceWaitingQueues_[resId];
    }
    return {};
}

std::vector<ResourceId> ResourceManager::getLocalResources() const
{
    std::vector<ResourceId> localResources;
    int startResId = (nodeId_ - 1) * RESOURCES_PER_NODE + 1;
    int endResId = nodeId_ * RESOURCES_PER_NODE;
    for (int i = startResId; i <= endResId; ++i)
    {
        localResources.push_back(i);
    }
    return localResources;
}

bool ResourceManager::checkConflict(ResourceId resId, LockMode requestMode)
{
    if (resourceHolders_.find(resId) == resourceHolders_.end() || resourceHolders_[resId].empty())
    {
        return false;
    }

    if (resourceWaitingQueues_.count(resId) && !resourceWaitingQueues_[resId].empty()) {
        return true;
    }

    for (const auto &holderPair : resourceHolders_[resId])
    {
        LockMode currentHolderMode = holderPair.second;

        if (requestMode == LockMode::SHARED)
        {
            if (currentHolderMode == LockMode::EXCLUSIVE)
            {
                return true;
            }
        }
        else if (requestMode == LockMode::EXCLUSIVE)
        {
            return true;
        }
    }
    return false;
}

bool ResourceManager::removeTransactionFromWaitingQueue(TransactionId transId, ResourceId resId)
{
    std::unique_lock<std::mutex> queues_lock(resourceWaitingQueuesMutex_);
    if (resourceWaitingQueues_.count(resId) && !resourceWaitingQueues_[resId].empty())
    {
        std::queue<TransactionId> tempQueue;
        bool found = false;
        while (!resourceWaitingQueues_[resId].empty())
        {
            TransactionId currentTrans = resourceWaitingQueues_[resId].front();
            resourceWaitingQueues_[resId].pop();
            if (currentTrans == transId)
            {
                found = true;
            }
            else
            {
                tempQueue.push(currentTrans);
            }
        }
        resourceWaitingQueues_[resId] = tempQueue;
        if (found)
        {
            std::cout << "Node " << nodeId_ << ": Removed Trans " << transId << " from R" << resId << " waiting queue.\n";
        }
        return found;
    }
    return false;
}