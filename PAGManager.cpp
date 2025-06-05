#include "PAGManager.h"
#include <algorithm>
#include <set>
#include <iostream>
#include <unordered_set>

void PAGManager::findSCCsDFS(NodeId u,
                             const PAG &graph,
                             std::unordered_map<NodeId, int> &disc,
                             std::unordered_map<NodeId, int> &low,
                             std::stack<NodeId> &st,
                             std::unordered_map<NodeId, bool> &onStack,
                             std::vector<std::vector<NodeId>> &sccs,
                             int &time)
{
    disc[u] = low[u] = ++time;
    st.push(u);
    onStack[u] = true;

    if (graph.count(u))
    {
        for (NodeId v : graph.at(u))
        {
            if (disc.find(v) == disc.end())
            {
                findSCCsDFS(v, graph, disc, low, st, onStack, sccs, time);
                low[u] = std::min(low[u], low[v]);
            }
            else if (onStack[v])
            {
                low[u] = std::min(low[u], disc[v]);
            }
        }
    }

    if (low[u] == disc[u])
    {
        std::vector<NodeId> scc;
        while (true)
        {
            NodeId node = st.top();
            st.pop();
            onStack[node] = false;
            scc.push_back(node);
            if (node == u)
                break;
        }
        std::reverse(scc.begin(), scc.end());
        sccs.push_back(scc);
    }
}

PAG PAGManager::generatePAG(const std::vector<WFDEdge> &sampledPagEdges)
{
    PAG pag;
    for (const auto &edge : sampledPagEdges)
    {
        if (edge.waitingNodeId != edge.holdingNodeId)
        {
            pag[edge.waitingNodeId].push_back(edge.holdingNodeId);
        }
    }
    std::cout << "PAGManager: Generated PAG with " << pag.size() << " nodes (unique waiting nodes).\n";
    return pag;
}

std::pair<std::vector<std::vector<NodeId>>, std::vector<NodeId>>
PAGManager::greedySCCcut(const PAG &pag, int threshold)
{
    std::vector<std::vector<NodeId>> sccs;
    std::unordered_map<NodeId, int> disc;
    std::unordered_map<NodeId, int> low;
    std::stack<NodeId> st;
    std::unordered_map<NodeId, bool> onStack;
    int time = 0;

    for (const auto &pair : pag)
    {
        disc[pair.first] = -1;
        onStack[pair.first] = false;
        for (NodeId neighbor : pair.second)
        {
            if (disc.find(neighbor) == disc.end())
            {
                disc[neighbor] = -1;
                onStack[neighbor] = false;
            }
        }
    }

    for (const auto &pair : pag)
    {
        if (disc[pair.first] == -1)
        {
            findSCCsDFS(pair.first, pag, disc, low, st, onStack, sccs, time);
        }
    }

    std::vector<std::vector<NodeId>> detectionZones;
    std::vector<NodeId> detectionZoneLeaders;

    std::cout << "PAGManager: Found " << sccs.size() << " SCCs before cutting.\n";

    for (const auto &scc : sccs)
    {
        if (scc.size() >= threshold)
        {
            detectionZones.push_back(scc);
            NodeId leader = scc[0];
            for (size_t i = 1; i < scc.size(); ++i) {
                if (scc[i] < leader) {
                    leader = scc[i];
                }
            }
            detectionZoneLeaders.push_back(leader);
            std::cout << "SCC kept: Zone leader " << leader << " for zone with " << scc.size() << " members.\n";
        }
    }

    std::unordered_set<NodeId> covered_nodes;
    for(const auto& zone : detectionZones) {
        for(NodeId node : zone) {
            covered_nodes.insert(node);
        }
    }

    for (const auto& pair : pag) {
        NodeId node = pair.first;
        if (covered_nodes.find(node) == covered_nodes.end()) {
            detectionZones.push_back({node});
            detectionZoneLeaders.push_back(node);
            covered_nodes.insert(node);
            std::cout << "Isolated node: Node " << node << " becomes its own detection zone.\n";
        }
    }

    std::cout << "Greedy SCC cut resulted in " << detectionZones.size() << " detection zones.\n";

    return {detectionZones, detectionZoneLeaders};
}