#ifndef HAWK_DETECTION_ZONE_MANAGER_H
#define HAWK_DETECTION_ZONE_MANAGER_H

#include "commons.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <algorithm>

class DetectionZoneManager
{
public:
    DetectionZoneManager(NodeId nodeId);

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
