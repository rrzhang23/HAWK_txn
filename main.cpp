#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>

#include "commons.h"
#include "Network.h"
#include "DistributedDBNode.h"

std::atomic<bool> systemRunning(true);

void runClientMode(NodeId serverNodeId)
{
    std::cout << "Starting Client Mode...\\n";
    std::cout << "Connecting to server node: " << serverNodeId << " at port " << BASE_PORT + serverNodeId << "\\n";

    Network clientNetwork(0, NUM_NODES, true);
    clientNetwork.connectToServer(serverNodeId);

    std::string command;
    while (systemRunning)
    {
        std::cout << "Enter command (collect, print, abort <transId>, exit): ";
        std::getline(std::cin, command);

        if (command == "exit")
        {
            break;
        }
        else if (command == "collect")
        {
            clientNetwork.sendCollectCommand(CENTRALIZED_NODE_ID);
            clientNetwork.receiveAndPrintResponse();
        }
        else if (command == "print")
        {
            clientNetwork.sendPrintCommand();
            clientNetwork.receiveAndPrintResponse();
        }
        else if (command.substr(0, 5) == "abort")
        {
            try
            {
                int transId = std::stoi(command.substr(6));
                clientNetwork.sendAbortCommand(transId);
                clientNetwork.receiveAndPrintResponse();
            }
            catch (const std::invalid_argument &e)
            {
                std::cerr << "Invalid transaction ID. Usage: abort <transaction_id>\\n";
            }
        }
        else
        {
            std::cout << "Unknown command.\\n";
        }\
    }

    std::cout << "Client mode gracefully shut down.\\n";
    systemRunning = false;
}

int main(int argc, char *argv[])
{
    if (argc > 1 && std::string(argv[1]) == "client")
    {
        if (argc != 3)
        {
            std::cerr << "Usage: " << argv[0] << " client <server_node_id>\\n";
            return 1;
        }
        NodeId serverNodeId = std::stoi(argv[2]);
        runClientMode(serverNodeId);
    }
    else if (argc > 1 && std::string(argv[1]) == "server")
    {
        if (argc != 3)
        {
            std::cerr << "Usage: " << argv[0] << " server <node_id>\\n";
            return 1;
        }
        NodeId nodeId = std::stoi(argv[2]);
        std::cout << "Starting Distributed Deadlock Detection System (Node " << nodeId << " Server Mode)...\\n";
        std::cout << "Number of nodes: " << NUM_NODES << "\\n";
        std::cout << "Resources per node: " << RESOURCES_PER_NODE << "\\n";
        std::cout << "Max concurrent transactions per node (polled): " << MAX_CONCURRENT_TRANSACTIONS_PER_NODE << "\\n";
        std::cout << "Centralized Node ID: " << CENTRALIZED_NODE_ID << "\\n";
        std::cout << "Node " << nodeId << " listening on port (gRPC): " << BASE_PORT + nodeId << "\\n";
        std::cout << "Total simulation run time: " << TOTAL_RUN_TIME_SECONDS << " seconds\\n";
        std::cout << "Deadlock Detection Mode: ";
        if (DEADLOCK_DETECTION_MODE == MODE_NONE) std::cout << "NONE\\n";
        else if (DEADLOCK_DETECTION_MODE == MODE_CENTRALIZED) std::cout << "CENTRALIZED\\n";
        else if (DEADLOCK_DETECTION_MODE == MODE_HAWK) std::cout << "HAWK\\n";
        else if (DEADLOCK_DETECTION_MODE == MODE_PATH_PUSHING) std::cout << "PATH_PUSHING\\n";
        std::cout << "Transaction Type: ";
#ifdef TRANSACTION_TYPE_TPCC
        std::cout << "TPC-C\\n";
#else
        std::cout << "Basic Random Transactions\\n";
#endif

        Network network(nodeId, NUM_NODES);
        DistributedDBNode node(nodeId, NUM_NODES, network);

        node.run();

        long long totalLatency = 0;
        int totalTransactions = 0;
        std::vector<long long> latencies = node.getCompletedTransactionLatencies();
        for (long long lat : latencies)
        {
            totalLatency += lat;
        }
        totalTransactions += latencies.size();

        if (totalTransactions > 0)
        {
            double averageLatency = static_cast<double>(totalLatency) / totalTransactions / 1000.0;
            std::cout << "Node " << nodeId << ": Total completed transactions: " << totalTransactions << "\\n";
            std::cout << "Node " << nodeId << ": Average transaction latency: " << averageLatency << " ms\\n";
        }
        else
        {
            std::cout << "Node " << nodeId << ": No transactions completed during the simulation.\\n";
        }
        std::cout << "Node " << nodeId << " gracefully shut down.\\n";
    }
    else
    {
        std::cerr << "Usage: " << argv[0] << " <server | client> <node_id | server_node_id>\\n";
        return 1;
    }

    return 0;
}

