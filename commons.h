#ifndef HAWK_CONSTANTS_H
#define HAWK_CONSTANTS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <set>
#include <unordered_set>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <cerrno>


#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif


using NodeId = int;
using TransactionId = int;
using ResourceId = int;


const int NUM_NODES = 128; // Total number of nodes in the distributed database system.
const int RESOURCES_PER_NODE = 1000; // Number of resources managed by each node.
const int TOTAL_RESOURCES = NUM_NODES * RESOURCES_PER_NODE; // Total number of resources across all nodes.


const double SQL_COUNT_LAMBDA = 1.0 / 30.0; // Lambda parameter for exponential distribution of SQL statements per transaction.
const int MIN_SQLS_PER_TRANSACTION = 10; // Minimum number of SQL statements per transaction.
const int MAX_SQLS_PER_TRANSACTION = 50; // Maximum number of SQL statements per transaction.


const double RESOURCE_REQUEST_LAMBDA = 1.0 / 1.2; // Lambda parameter for exponential distribution of resource requests per SQL statement.
const int MIN_RESOURCES_PER_SQL = 1; // Minimum number of resources requested per SQL statement.
const int MAX_RESOURCES_PER_SQL = 5; // Maximum number of resources requested per SQL statement.


const int MAX_CONCURRENT_TRANSACTIONS_PER_NODE = 8; // Maximum number of transactions concurrently active on a single node.
// const int TRANSACTION_POLLING_INTERVAL_MS = 1;


const double EXCLUSIVE_LOCK_PROBABILITY = 0.5; // Probability of an exclusive lock request.


const int DEADLOCK_DETECTION_INTERVAL_MS = 50;

// TPC-C specific constants
const int WAREHOUSES_PER_NODE = 10;
const int NUM_WAREHOUSES = NUM_NODES * WAREHOUSES_PER_NODE;

const int PAG_SAMPLE_INTERVAL_MS = 5000;
// const int TREE_ADJUST_INTERVAL_MS = 60000;
const int SCC_CUT_THRESHOLD = 2;

const int MONITORING_INTERVAL_MS = 2000;

const int TOTAL_RUN_TIME_SECONDS = 1800;

// Network configuration
const int BASE_PORT = 8000;
const int CLIENT_PORT = 9000;
const std::string NODE_IP_PREFIX = "127.0.0.1";

const NodeId CENTRALIZED_NODE_ID = 1;


enum DeadlockDetectionMode {
    MODE_NONE = 0, 
    MODE_CENTRALIZED = 1,  // Centralized deadlock detection mode.
    MODE_HAWK = 2,   // HAWK (Hierarchical Adaptive Wait-for Graph) deadlock detection mode.
    MODE_PATH_PUSHING = 3  // Path Pushing deadlock detection mode.
};


const DeadlockDetectionMode DEADLOCK_DETECTION_MODE = MODE_CENTRALIZED;

// --- Transaction Type Control Macros ---
// Define transaction type, only one can be selected
// #define TRANSACTION_TYPE_RANDOM     // Random transactions
#define TRANSACTION_TYPE_TPCC       // TPC-C transactions
// #define TRANSACTION_TYPE_CONTROLLED // Controlled transactions (for deadlock examples)
const int kTRANSACTION_TYPE_TPCC = 1; // Set to 1 to enable TPC-C transactions, 0 for generic transactions (this line remains for its original purpose)


enum class LockMode
{
    SHARED,   
    EXCLUSIVE 
};

// Represents a single SQL statement within a transaction.
struct SQLStatement
{
    TransactionId transId; // The ID of the transaction to which this SQL statement belongs.
    NodeId homeNodeId; // The home node of the transaction.
    std::vector<ResourceId> resources; // List of resources requested by this SQL statement.
    LockMode lockMode;            // The type of lock requested (SHARED or EXCLUSIVE).
};



// Types of network messages exchanged between nodes for various operations,
// including lock management, PAG updates, and deadlock detection.
enum NetworkMessageType
{
    UNKNOWN = 0, 

    // Transaction/Lock Management
    LOCK_REQUEST = 1,
    LOCK_RESPONSE = 2,
    RELEASE_LOCK_REQUEST = 3,
    RELEASE_LOCK_RESPONSE = 4,

    // PAG/WFG related
    PAG_REQUEST = 5,
    PAG_RESPONSE = 6,
    WFG_REPORT = 7,

    // Deadlock Resolution
    DEADLOCK_RESOLUTION = 8,
    ABORT_TRANSACTION_SIGNAL = 9,

    // Hierarchical/Zonal Detection (HAWK)
    DISTRIBUTED_DETECTION_INIT = 10, 
    ZONE_DETECTION_REQUEST = 11,
    ZONE_WFG_REPORT = 12,
    CENTRAL_WFG_REPORT_FROM_ZONE = 13,

    // Path-Pushing Detection
    PATH_PUSHING_PROBE = 14,

    // Client-Server Communication
    CLIENT_COLLECT_WFG_REQUEST = 15,
    CLIENT_COLLECT_WFG_RESPONSE = 16,
    CLIENT_PRINT_DEADLOCK_REQUEST = 17,
    CLIENT_RESOLVE_DEADLOCK_REQUEST = 18,
    DEADLOCK_REPORT_TO_CLIENT = 19
};


// Represents an edge in the Wait-For Graph (WFG) or PAG.
struct WFDEdge
{
    TransactionId waitingTransId;
    TransactionId holdingTransId;
    NodeId waitingNodeId; 
    NodeId holdingNodeId; 
};


// Structure for network messages, containing various fields depending on the message type.
// This union-like structure allows different types of data to be carried by a single message.
struct NetworkMessage
{
    NetworkMessageType type;
    NodeId senderId;
    NodeId receiverId; // 0 for broadcast


    
    TransactionId transId;
    ResourceId resId;
    LockMode mode; 
    bool granted; 


    std::unordered_map<TransactionId, std::vector<TransactionId>> wfgData; 


    std::vector<std::pair<TransactionId, std::vector<TransactionId>>> wfgDataPairs;


    std::vector<TransactionId> deadlockedTransactions;


    std::vector<WFDEdge> pagEdges;


    std::vector<std::vector<NodeId>> detectionZones; 
    std::vector<NodeId> detectionZoneLeaders; 

    TransactionId victimTransId;

  
    std::vector<std::vector<TransactionId>> detectedCycles; 
    int deadlockCount; 


    std::vector<TransactionId> path;


    NodeId centralNodeId; 
    std::vector<NodeId> zoneMembers; 
};

// Utility function to determine the owner node of a given resource.
// Resources are distributed among nodes based on their ID.
inline NodeId getOwnerNodeId(ResourceId resId)
{
    return (resId - 1) / RESOURCES_PER_NODE + 1;
}

const int NUM_DOMAINS = 16; // Number of zones (for TPCC partitioning).
const int NODES_PER_DOMAIN = NUM_NODES / NUM_DOMAINS;
const double DOMAIN_LOCAL_ACCESS_PROBABILITY = 0.80;
const double DOMAIN_REMOTE_ACCESS_PROBABILITY = 0.20; 

inline int getDomainId(NodeId nodeId) {
    return (nodeId - 1) / NODES_PER_DOMAIN;
}
#endif // HAWK_CONSTANTS_H
