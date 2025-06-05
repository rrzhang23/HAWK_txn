#!/bin/bash

NUM_NODES=$(grep -m 1 "const int NUM_NODES =" commons.h | awk '{print $NF}' | sed 's/;//')

CENTRALIZED_NODE_ID=$(grep -m 1 "const int CENTRALIZED_NODE_ID =" commons.h | awk '{print $NF}' | sed 's/;//')

TOTAL_RUN_TIME_SECONDS=$(grep -m 1 "const int TOTAL_RUN_TIME_SECONDS =" commons.h | awk '{print $NF}' | sed 's/;//')

GRPC_INSTALL_DIR="/home/user/software/grpc-v1.59.0"

IP_PREFIX="10.181.81."

# --- Cleanup previous runs ---
echo "Cleaning up previous runs..."
make clean
rm -f nohup.out
echo "Cleanup complete."

# --- Compile the project ---
echo "Compiling the project..."
make all
if [ $? -ne 0 ]; then
  echo "Compilation failed. Exiting."
  exit 1
fi
echo "Compilation successful."


REMOTE_PIDS_FILE="remote_node_pids.txt"
> "$REMOTE_PIDS_FILE"

for (( i=1; i<=$NUM_NODES; i++ ))
do
    IP_ADDRESS="${IP_PREFIX}${i}"
    echo "Starting node $i on ${IP_ADDRESS}..."
    SSH_COMMAND="cd $TARGET_DIR && nohup ./distributed_deadlock_detector server $i > node_$i.log 2>&1 & echo \$!"
    
    REMOTE_PID=$(ssh "$IP_ADDRESS" "$SSH_COMMAND")
    if [ $? -eq 0 ] && [ -n "$REMOTE_PID" ]; then
        echo "Node $i started on ${IP_ADDRESS} with PID: $REMOTE_PID"
        echo "$IP_ADDRESS:$REMOTE_PID" >> "$REMOTE_PIDS_FILE"
    else
        echo "Error: Failed to start node $i on ${IP_ADDRESS}."
    fi
    sleep 0.1
done

sleep $TOTAL_RUN_TIME_SECONDS

echo "Simulation time elapsed. Sending termination signal to all remote nodes."

# Terminate remote processes using the stored PIDs
while IFS= read -r line || [[ -n "$line" ]]; do
    IP_ADDRESS=$(echo "$line" | cut -d':' -f1)
    PID=$(echo "$line" | cut -d':' -f2)
    if [ -n "$IP_ADDRESS" ] && [ -n "$PID" ]; then
        echo "Killing PID $PID on ${IP_ADDRESS}..."
        ssh "$IP_ADDRESS" "kill $PID" &
    fi
done < "$REMOTE_PIDS_FILE"
wait
echo "All remote nodes terminated."
echo "Simulation finished."
