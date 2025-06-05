#include "DeadlockDetector.h"
#include <iostream>
#include <algorithm> 
#include <cmath>    


void DeadlockDetector::dfs(TransactionId u,
                         const std::unordered_map<TransactionId, std::vector<TransactionId>> &graph,
                         std::unordered_map<TransactionId, int> &visited_count, 
                         std::unordered_map<TransactionId, bool> &recursionStack,
                         std::unordered_map<TransactionId, TransactionId> &parent,
                         std::vector<std::vector<TransactionId>> &cycles,
                         std::unordered_map<TransactionId, int> &frequency)
{
    visited_count[u]--;

    recursionStack[u] = true;

    if (graph.count(u))
    {
        for (TransactionId v : graph.at(u))
        {
            if (visited_count[v] > 0)
            {
                parent[v] = u;
                dfs(v, graph, visited_count, recursionStack, parent, cycles, frequency);
            }
            else if (recursionStack[v])
            {
                std::vector<TransactionId> cycle;
                TransactionId curr = u;
                while (curr != v)
                {
                    cycle.push_back(curr);
                    frequency[curr]++; 
                    curr = parent[curr];
                }
                cycle.push_back(v);
                frequency[v]++;
                std::reverse(cycle.begin(), cycle.end());
                cycles.push_back(cycle);
            }
        }
    }
    recursionStack[u] = false; 
}

std::pair<std::vector<std::vector<TransactionId>>, std::unordered_map<TransactionId, int>>
DeadlockDetector::findCycles(const std::unordered_map<TransactionId, std::vector<TransactionId>> &graph)
{
    std::vector<std::vector<TransactionId>> cycles;
    std::unordered_map<TransactionId, int> visited_count; 
    std::unordered_map<TransactionId, bool> recursionStack;
    std::unordered_map<TransactionId, TransactionId> parent;
    std::unordered_map<TransactionId, int> frequency; 

    std::unordered_map<TransactionId, int> in_degree;
    std::unordered_map<TransactionId, int> out_degree;

    for (const auto &pair : graph)
    {
        TransactionId u = pair.first;
        out_degree[u] = pair.second.size(); 
        for (TransactionId v : pair.second)
        {
            in_degree[v]++;
        }
    }

    for (const auto &pair : graph)
    {
        TransactionId txn_id = pair.first;
        recursionStack[txn_id] = false;
        parent[txn_id] = 0;
        frequency[txn_id] = 0;

        visited_count[txn_id] = std::abs(out_degree[txn_id] - in_degree[txn_id]) + 1;
    }
    for (const auto &pair : graph) {
        for (TransactionId target : pair.second) {
            if (visited_count.find(target) == visited_count.end()) {
                visited_count[target] = std::abs(out_degree[target] - in_degree[target]) + 1;
                recursionStack[target] = false;
                parent[target] = 0;
                frequency[target] = 0;
            }
        }
    }

    for (const auto &pair : graph)
    {
        TransactionId txn_id = pair.first;
        {
            dfs(txn_id, graph, visited_count, recursionStack, parent, cycles, frequency);
        }
    }
    return {cycles, frequency};
}

bool DeadlockDetector::compareTransactionPriority(const std::pair<TransactionId, int> &a,
                                                 const std::pair<TransactionId, int> &b)
{
    if (a.second != b.second)
    {
        return a.second > b.second; 
    }
    return a.first < b.first;      
}
