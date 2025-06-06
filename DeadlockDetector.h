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

    // Finds all cycles (deadlocks) in the given Wait-For Graph.
    // In HAWK, this function would be called by zone leaders or the central node
    // on their respective aggregated WFGs.
    // graph: The Wait-For Graph represented as an adjacency list.
    // Returns: A pair containing a vector of detected cycles and a map of transaction
    //          frequencies in the cycles (how many cycles each transaction participates in).
    std::pair<std::vector<std::vector<TransactionId>>, std::unordered_map<TransactionId, int>>
    findCycles(const std::unordered_map<TransactionId, std::vector<TransactionId>> &graph);

    // Compares transaction priorities for deadlock resolution.
    // This is a static function that can be used to select a victim transaction
    // based on certain criteria (e.g., transaction ID, number of cycles involved).
    static bool compareTransactionPriority(const std::pair<TransactionId, int> &a,
                                          const std::pair<TransactionId, int> &b);

private:
    // Depth-First Search (DFS) utility function to detect cycles in a graph.
    // This recursive function traverses the graph, keeping track of visited nodes
    // and nodes currently in the recursion stack to identify back-edges, which indicate cycles.
    // In HAWK, this DFS is crucial for identifying deadlocks within a zone's WFG
    // or the globally aggregated WFG.
    // u: Current transaction being visited.
    // graph: The WFG.
    // visited_count: Keeps track of how many times a node has been visited to handle complex graphs.
    // recursionStack: Marks nodes currently in the recursion stack to detect back-edges.
    // parent: Stores the parent of each node in the DFS tree to reconstruct cycles.
    // cycles: Stores detected deadlock cycles.
    // frequency: Counts how many cycles each transaction is part of.
    void dfs(TransactionId u,
             const std::unordered_map<TransactionId, std::vector<TransactionId>> &graph,
             std::unordered_map<TransactionId, int> &visited_count,
             std::unordered_map<TransactionId, bool> &recursionStack,
             std::unordered_map<TransactionId, TransactionId> &parent,
             std::vector<std::vector<TransactionId>> &cycles,
             std::unordered_map<TransactionId, int> &frequency);
};

#endif // HAWK_DEADLOCK_DETECTOR_H
