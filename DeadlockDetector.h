#ifndef HAWK_DEADLOCK_DETECTOR_H
#define HAWK_DEADLOCK_DETECTOR_H

#include "commons.h"
#include <vector>
#include <unordered_map>
#include <stack>   
#include <algorithm> 
class DeadlockDetector
{
public:
    DeadlockDetector() = default;

    std::pair<std::vector<std::vector<TransactionId>>, std::unordered_map<TransactionId, int>>
    findCycles(const std::unordered_map<TransactionId, std::vector<TransactionId>> &graph);

    static bool compareTransactionPriority(const std::pair<TransactionId, int> &a,
                                          const std::pair<TransactionId, int> &b);

private:
    void dfs(TransactionId u,
             const std::unordered_map<TransactionId, std::vector<TransactionId>> &graph,
             std::unordered_map<TransactionId, int> &visited_count,
             std::unordered_map<TransactionId, bool> &recursionStack,
             std::unordered_map<TransactionId, TransactionId> &parent,
             std::vector<std::vector<TransactionId>> &cycles,
             std::unordered_map<TransactionId, int> &frequency);
};

#endif // HAWK_DEADLOCK_DETECTOR_H
