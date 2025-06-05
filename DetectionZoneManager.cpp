#include "DetectionZoneManager.h"
#include <iostream> 

DetectionZoneManager::DetectionZoneManager(NodeId nodeId) : nodeId(nodeId), myZoneLeaderId_(nodeId)
{
    detectionZones[nodeId].push_back(nodeId);
    myDetectionZoneMembers_.push_back(nodeId);
}

void DetectionZoneManager::updateDetectionZones(const std::vector<std::vector<NodeId>> &newZones, const std::vector<NodeId>& newLeaders)
{
    std::unique_lock<std::mutex> lock(zoneMutex); 
    detectionZones.clear(); 
    myDetectionZoneMembers_.clear(); 
    myZoneLeaderId_ = 0;

    for (size_t i = 0; i < newZones.size(); ++i)
    {
        const auto &zone = newZones[i];
        NodeId leader = newLeaders[i];

        if (!zone.empty())
        {
            detectionZones[leader] = zone; 

            if (std::find(zone.begin(), zone.end(), nodeId) != zone.end())
            {
                myZoneLeaderId_ = leader;
                myDetectionZoneMembers_ = zone;
            }
        }
    }
    std::cout << "Node " << nodeId << ": Detection zones updated. My leader: " << myZoneLeaderId_ << ", My zone members: ";
    for(NodeId member : myDetectionZoneMembers_) {
        std::cout << member << " ";
    }
    std::cout << "\n";
}

std::unordered_map<NodeId, std::vector<NodeId>> DetectionZoneManager::getCurrentDetectionZones()
{
    std::unique_lock<std::mutex> lock(zoneMutex); 
    return detectionZones; 
}

NodeId DetectionZoneManager::getMyZoneLeaderId()
{
    std::unique_lock<std::mutex> lock(zoneMutex);
    return myZoneLeaderId_;
}

bool DetectionZoneManager::isZoneLeader()
{
    std::unique_lock<std::mutex> lock(zoneMutex);
    return nodeId == myZoneLeaderId_;
}

const std::vector<NodeId>& DetectionZoneManager::getMyDetectionZoneMembers()
{
    std::unique_lock<std::mutex> lock(zoneMutex);
    return myDetectionZoneMembers_;
}
