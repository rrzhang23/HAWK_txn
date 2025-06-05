#pragma once

#include "tpcc.h"
#include "LockTable.h"
#include "commons.h"
#include "tpcc_data_generator.h"
#include "Transaction.h"
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <unordered_set>

const ResourceId TPCC_RESOURCE_BASE_WAREHOUSE = 1000000;
const ResourceId TPCC_RESOURCE_BASE_DISTRICT = 2000000;
const ResourceId TPCC_RESOURCE_BASE_CUSTOMER = 3000000;
const ResourceId TPCC_RESOURCE_BASE_ITEM = 4000000;
const ResourceId TPCC_RESOURCE_BASE_STOCK = 5000000;
const ResourceId TPCC_RESOURCE_BASE_ORDER = 6000000;
const ResourceId TPCC_RESOURCE_BASE_NEW_ORDER = 7000000;
const ResourceId TPCC_RESOURCE_BASE_ORDER_LINE = 8000000;
const ResourceId TPCC_RESOURCE_BASE_HISTORY = 9000000;

inline ResourceId getTPCCResourceId(const std::string& table_name, int w_id, int d_id = 0, int c_id = 0, int item_id = 0, int o_id = 0, int ol_number = 0) {
    if (table_name == "WAREHOUSE") {
        return TPCC_RESOURCE_BASE_WAREHOUSE + w_id;
    } else if (table_name == "DISTRICT") {
        return TPCC_RESOURCE_BASE_DISTRICT + (w_id - 1) * 10 + d_id;
    } else if (table_name == "CUSTOMER") {
        return TPCC_RESOURCE_BASE_CUSTOMER + (w_id - 1) * 10 * 3000 + (d_id - 1) * 3000 + c_id;
    } else if (table_name == "ITEM") {
        return TPCC_RESOURCE_BASE_ITEM + item_id;
    } else if (table_name == "STOCK") {
        return TPCC_RESOURCE_BASE_STOCK + (w_id - 1) * 100000 + item_id;
    } else if (table_name == "ORDER") {
        return TPCC_RESOURCE_BASE_ORDER + (w_id - 1) * 10 * 3000 + (d_id - 1) * 3000 + o_id;
    } else if (table_name == "NEW_ORDER") {
        return TPCC_RESOURCE_BASE_NEW_ORDER + (w_id - 1) * 10 * 900 + (d_id - 1) * 900 + o_id;
    } else if (table_name == "ORDER_LINE") {
        return TPCC_RESOURCE_BASE_ORDER_LINE + (w_id - 1) * 10 * 3000 * 15 + (d_id - 1) * 3000 * 15 + (o_id - 1) * 15 + ol_number;
    } else if (table_name == "HISTORY") {
        return TPCC_RESOURCE_BASE_HISTORY + (w_id - 1) * 10 * 3000 + (d_id - 1) * 3000 + c_id;
    }
    return 0;
}

class TPCCTransaction : public Transaction {
public:
    TPCCDatabase& db_;
    LockTable& lock_table_;
    NodeId home_node_id_;

    TPCCRandom& rng_;

    TPCCTransaction(TPCCDatabase& db, LockTable& lock_table, TransactionId txn_id, NodeId home_node_id, TPCCRandom& rng)
        : Transaction(), db_(db), lock_table_(lock_table), home_node_id_(home_node_id), rng_(rng) {
        this->id = txn_id;
        this->homeNodeId = home_node_id;
        this->startTime = std::chrono::high_resolution_clock::now();
        this->status = TransactionStatus::RUNNING;
    }

    virtual bool execute() = 0;

protected:
    bool acquireLock(ResourceId resId, LockMode mode) {
        bool success = lock_table_.acquireLock(this->id, resId, mode);
        if (success) {
            this->acquiredLocks[resId] = mode;
        } else {
            this->status = TransactionStatus::BLOCKED;
            this->waitingForResourceId = resId;
        }
        return success;
    }

    void releaseAllLocks() {
        lock_table_.releaseAllLocks(this->id);
        this->acquiredLocks.clear();
        this->status = TransactionStatus::COMMITTED;
    }

    void abort() {
        std::cout << "Trans " << this->id << ": TPC-C transaction aborted.\n";
        releaseAllLocks();
        this->status = TransactionStatus::ABORTED;
    }
};

class TPCCNewOrderTransaction : public TPCCTransaction {
public:
    TPCCNewOrderTransaction(TPCCDatabase& db, LockTable& lock_table,
                            TransactionId txn_id, NodeId home_node_id, TPCCRandom& rng,
                            int w_id, int d_id, int c_id,
                            const std::vector<std::pair<int, int>>& item_info);

    bool execute() override;

private:
    int w_id_;
    int d_id_;
    int c_id_;
    std::vector<std::pair<int, int>> item_info_;
};

class TPCCPaymentTransaction : public TPCCTransaction {
public:
    TPCCPaymentTransaction(TPCCDatabase& db, LockTable& lock_table,
                           TransactionId txn_id, NodeId home_node_id, TPCCRandom& rng,
                           int w_id, int d_id, int c_w_id, int c_d_id, int c_id, double h_amount);

    bool execute() override;

private:
    int w_id_;
    int d_id_;
    int c_w_id_;
    int c_d_id_;
    int c_id_;
    double h_amount_;
    bool by_last_name_;
};

class TPCCOrderStatusTransaction : public TPCCTransaction {
public:
    TPCCOrderStatusTransaction(TPCCDatabase& db, LockTable& lock_table,
                               TransactionId txn_id, NodeId home_node_id, TPCCRandom& rng,
                               int w_id, int d_id, int c_id);

    bool execute() override;

private:
    int w_id_;
    int d_id_;
    int c_id_;
    bool by_last_name_;
};

class TPCCDeliveryTransaction : public TPCCTransaction {
public:
    TPCCDeliveryTransaction(TPCCDatabase& db, LockTable& lock_table,
                            TransactionId txn_id, NodeId home_node_id, TPCCRandom& rng,
                            int w_id, int o_carrier_id);

    bool execute() override;

private:
    int w_id_;
    int o_carrier_id_;
};

class TPCCStockLevelTransaction : public TPCCTransaction {
public:
    TPCCStockLevelTransaction(TPCCDatabase& db, LockTable& lock_table,
                              TransactionId txn_id, NodeId home_node_id, TPCCRandom& rng,
                              int w_id, int d_id, int threshold);

    bool execute() override;

private:
    int w_id_;
    int d_id_;
    int threshold_;
};