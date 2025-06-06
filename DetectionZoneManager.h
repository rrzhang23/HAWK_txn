#ifndef HAWK_DETECTION_ZONE_MANAGER_H
#define HAWK_DETECTION_ZONE_MANAGER_H

#include "commons.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <algorithm>
// DetectionZoneManager manages the detection zones in the HAWK deadlock detection scheme.
// Each node uses this manager to understand its own zone, its zone leader, and the members
// of its zone, which is crucial for the hierarchical and adaptive nature of HAWK.
class DetectionZoneManager
{
public:
    DetectionZoneManager(NodeId nodeId);
    // Updates the detection zones and their leaders based on information received
    // from a higher-level coordinator (e.g., the central node in HAWK).
    // This is a key function in HAWK as it allows zones to be dynamically reconfigured.
    // newZones: A vector of vectors, where each inner vector represents a detection zone.
    // newLeaders: A vector containing the leader NodeId for each corresponding detection zone.
    void updateDetectionZones(const std::vector<std::vector<NodeId>> &newZones, const std::vector<NodeId>& newLeaders);

    std::unordered_map<NodeId, std::vector<NodeId>> getCurrentDetectionZones();

    NodeId getMyZoneLeaderId();

    bool isZoneLeader();

    const std::vector<NodeId>& getMyDetectionZoneMembers();

private:
    NodeId nodeId;
    std::unordered_map<NodeId, std::vector<NodeId>> detectionZones; 
    std::mutex zoneMutex; 

    NodeId myZoneLeaderId_; 
    std::vector<NodeId> myDetectionZoneMembers_; 
};

#endif // HAWK_DETECTION_ZONE_MANAGER_H
