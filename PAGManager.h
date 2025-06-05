#ifndef HAWK_PAG_MANAGER_H
#define HAWK_PAG_MANAGER_H

#include "commons.h"
#include <vector>
#include <unordered_map>
#include <stack>
#include <algorithm>

using PAG = std::unordered_map<NodeId, std::vector<NodeId>>;

class PAGManager
{
public:
    PAGManager() = default;

    PAG generatePAG(const std::vector<WFDEdge> &sampledPagEdges);

    std::pair<std::vector<std::vector<NodeId>>, std::vector<NodeId>>
    greedySCCcut(const PAG &pag, int threshold);

private:
    void findSCCsDFS(NodeId u,
                     const PAG &graph,
                     std::unordered_map<NodeId, int> &disc,
                     std::unordered_map<NodeId, int> &low,
                     std::stack<NodeId> &st,
                     std::unordered_map<NodeId, bool> &onStack,
                     std::vector<std::vector<NodeId>> &sccs,
                     int &timer);
};

#endif // HAWK_PAG_MANAGER_H