# HAWK: A Workload-driven Hierarchical Deadlock Detection Approach in Distributed Database System

HAWK is an innovative workload-driven hierarchical deadlock detection algorithm designed for distributed database systems. It addresses scalability and efficiency challenges by dynamically constructing a hierarchical detection tree based on transaction patterns, significantly reducing time complexity and communication overhead.

## Environment Setup
Before compiling and running the project, you need to install gRPC and Protobuf.

Install gRPC and Protobuf:
Please refer to the official gRPC documentation for installation.


## Compiling the Project
From the project root directory, execute the `make` command:
```
make
```
This will:

1. Generate gRPC and Protobuf C++ source and header files into the `generated_protos/` directory based on the `protos/network.proto` file.

2. Compile all C++ source files and the generated protobuf files.

3. Link to produce the final executable `distributed_deadlock_detector`.

## Running the benchmark
### Starting All Server Nodes (Recommended)
Use the provided `start_nodes.sh` script to conveniently launch all node processes.
1. 
Grant execute permissions to the script:
```
chmod +x start_nodes.sh
```

2. Run the script:
```
./start_nodes.sh
```
The script will:

- Execute make clean and make all.

- Create a protos/ directory (if it doesn't exist) and move network.proto into it.

- Launch a separate ./distributed_deadlock_detector server <node_id> process for each of the NUM_NODES nodes, running them in the background.

- Each node's output will be redirected to a node_<node_id>.log file.


### Manually Starting a Single Server Node
You can also manually start each node (e.g., in different terminal windows). This is useful for debugging individual nodes.
```
./distributed_deadlock_detector server <node_id>
```