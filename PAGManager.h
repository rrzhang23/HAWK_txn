#ifndef HAWK_PAG_MANAGER_H
#define HAWK_PAG_MANAGER_H

#include "commons.h"
#include <vector>
#include <unordered_map>
#include <stack>
#include <algorithm>
// PAG is a graph where nodes are database nodes
// and edges represent cross-node dependencies between transactions.
// This graph is central to the HAWK detection zone formation.
using PAG = std::unordered_map<NodeId, std::vector<NodeId>>;

class PAGManager
{
public:
    PAGManager() = default;
    // Generates the PAG from a list of sampled cross-node WFDEdges.
    // In HAWK, nodes periodically exchange their cross-node dependencies,
    // which are then used by the central node to construct this global PAG.
    // sampledPagEdges: A list of WFDEdges representing dependencies between nodes.
    // Returns: The constructed PAG.
    PAG generatePAG(const std::vector<WFDEdge> &sampledPagEdges);
    // Implements the greedy SCC (Strongly Connected Component) cutting algorithm.
    // This function takes the constructed PAG and a threshold to identify
    // strongly connected components (SCCs) within the PAG. These SCCs are then
    // potentially designated as detection zones in HAWK if they meet the size threshold.
    // pag: The Precedence-Augmented Graph.
    // threshold: Minimum size for an SCC to be considered a significant detection zone.
    // Returns: A pair containing a vector of detection zones (each a vector of NodeIds)
    //          and a vector of their corresponding zone leaders.
    std::pair<std::vector<std::vector<NodeId>>, std::vector<NodeId>>
    greedySCCcut(const PAG &pag, int threshold);

private:
    // Helper function for Tarjan's algorithm (or a similar DFS-based algorithm) to find SCCs.
    // This is a recursive DFS function that computes discovery times (disc) and low-link values (low)
    // to identify SCCs in the PAG. SCCs are then used by `greedySCCcut` to define detection zones.
    // u: The current node being visited.
    // graph: The PAG.
    // disc: Discovery times of nodes.
    // low: Lowest discovery time reachable from node u.
    // st: Stack used to store nodes currently in the recursion path.
    // onStack: Boolean map indicating if a node is on the stack.
    // sccs: Vector to store the identified SCCs.
    // timer: Global timer for discovery times.
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