#ifndef HAWK_TRANSACTION_H
#define HAWK_TRANSACTION_H

#include "commons.h"
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <memory>

enum class TransactionStatus
{
    RUNNING,
    BLOCKED,
    COMMITTED,
    ABORTED
};

struct Transaction
{
    virtual ~Transaction() = default;

    TransactionId id;
    NodeId homeNodeId;
    std::vector<SQLStatement> statements;
    TransactionStatus status = TransactionStatus::RUNNING;
    std::chrono::high_resolution_clock::time_point startTime;
    std::unordered_map<ResourceId, LockMode> acquiredLocks;
    int currentSQLIndex = 0;

    mutable std::mutex remoteRequestMutex;
    mutable std::condition_variable remoteRequestCv;
    bool remoteRequestPending = false;
    bool remoteRequestSuccess = false;

    mutable std::mutex localWaitMutex;
    mutable std::condition_variable localWaitCv;
    ResourceId waitingForResourceId = 0;
};

#endif // HAWK_TRANSACTION_H