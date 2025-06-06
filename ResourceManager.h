#ifndef HAWK_RESOURCE_MANAGER_H
#define HAWK_RESOURCE_MANAGER_H

#include "commons.h"
#include <unordered_map>
#include <queue>
#include <mutex>
#include <functional>

// ResourceManager is responsible for managing local resources and handling lock requests
// and releases for those resources. It maintains who holds which locks and who is waiting.
// This component forms the foundation for building the Wait-For Graph (WFG) used
// in various deadlock detection schemes, including HAWK.
class ResourceManager
{
public:
    ResourceManager(NodeId nodeId);

    bool acquireLock(TransactionId transId, ResourceId resId, LockMode mode);
    void releaseLock(TransactionId transId, ResourceId resId);
    void releaseAllLocks(TransactionId transId);

    std::unordered_map<TransactionId, LockMode> getResourceHolders(ResourceId resId);
    std::queue<TransactionId> getResourceWaitingQueue(ResourceId resId);

    std::vector<ResourceId> getLocalResources() const;

    std::function<void(TransactionId, ResourceId)> notifyTransactionToRetryAcquire;

    bool removeTransactionFromWaitingQueue(TransactionId transId, ResourceId resId);

private:
    NodeId nodeId_;

    std::unordered_map<ResourceId, std::unordered_map<TransactionId, LockMode>> resourceHolders_;
    std::mutex resourceHoldersMutex_;

    std::unordered_map<ResourceId, std::queue<TransactionId>> resourceWaitingQueues_;
    std::mutex resourceWaitingQueuesMutex_;

    bool checkConflict(ResourceId resId, LockMode requestMode);
};

#endif // HAWK_RESOURCE_MANAGER_H