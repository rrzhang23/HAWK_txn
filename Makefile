# --- gRPC and Protobuf Configuration ---
# Set these paths according to your installation
GRPC_INSTALL_DIR = /home/zhangrongrong/software/grpc-v1.59.0

# Path to protoc and grpc_cpp_plugin
# Assuming protoc is directly in GRPC_INSTALL_DIR/bin
PROTOC = $(GRPC_INSTALL_DIR)/bin/protoc
GRPC_CPP_PLUGIN = $(GRPC_INSTALL_DIR)/bin/grpc_cpp_plugin # Adjust path if needed, e.g., /home/zhangrongrong/software/grpc-v1.59.0/grpc/cmake/build/grpc_cpp_plugin

# Include directories for compilation
CXXFLAGS += -I$(GRPC_INSTALL_DIR)/include
CXXFLAGS += -I./generated_protos # Directory for generated protobuf headers

# Library directories for linking
LDFLAGS += -L$(GRPC_INSTALL_DIR)/lib

# Libraries to link
# -lgrpc++: C++ gRPC library
# -lgrpc: C-core gRPC library
# -lprotobuf: Protobuf library (now expected in GRPC_INSTALL_DIR/lib)
# -lpthread: POSIX threads library (for threading)
# -ldl: Dynamic linking library (often needed by gRPC for dlopen/dlsym)
# -lrt: Realtime extensions library (sometimes needed for high-resolution timers, depends on system)
LIBS = -lgrpc++ -lgrpc -lprotobuf -lpthread -ldl

# --- Source Files ---
# List all existing .cpp files
SRCS_CPP = \
    DeadlockDetector.cpp \
    DetectionZoneManager.cpp \
    DistributedDBNode.cpp \
    LockTable.cpp \
    main.cpp \
    Network.cpp \
    PAGManager.cpp \
    RandomGenerators.cpp \
    ResourceManager.cpp \
    tpcc_data_generator.cpp \
    tpcc_transaction.cpp \
    TransactionManager.cpp

# Add generated protobuf and gRPC source files
GENERATED_PROTO_SRCS = \
    generated_protos/network.pb.cc \
    generated_protos/network.grpc.pb.cc

# All .cpp files to be compiled
ALL_CPP_SRCS = $(SRCS_CPP) $(GENERATED_PROTO_SRCS)

# Object files
OBJS = $(patsubst %.cpp, %.o, $(ALL_CPP_SRCS))

# Executable name
TARGET = distributed_deadlock_detector

# --- Build Rules ---

.PHONY: all clean protos

all: protos $(TARGET) # Add 'protos' to ensure it runs first

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)

# Rule to generate protobuf and gRPC C++ files
protos:
	@echo "Generating Protobuf and gRPC C++ files..."
	mkdir -p generated_protos
	$(PROTOC) --proto_path=protos --cpp_out=generated_protos --grpc_out=generated_protos --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN) protos/network.proto
	@echo "Protobuf and gRPC files generated in generated_protos/."

# Generic rule to compile .cpp files to .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Dependencies for generated files: ensure they are generated before compilation
# This ensures that if protos/network.proto changes, the generated files are updated.
generated_protos/network.pb.cc: protos/network.proto
	$(MAKE) protos

generated_protos/network.grpc.pb.cc: protos/network.proto
	$(MAKE) protos

clean:
	@echo "Cleaning..."
	rm -f $(OBJS) $(TARGET)
	rm -rf generated_protos
	@echo "Cleaning complete."

