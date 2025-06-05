#include "tpcc_transaction.h"
#include <iostream>
#include <algorithm>
#include <numeric>

TPCCNewOrderTransaction::TPCCNewOrderTransaction(TPCCDatabase& db, LockTable& lock_table,
                                                 TransactionId txn_id, NodeId home_node_id, TPCCRandom& rng,
                                                 int w_id, int d_id, int c_id,
                                                 const std::vector<std::pair<int, int>>& item_info)
    : TPCCTransaction(db, lock_table, txn_id, home_node_id, rng),
      w_id_(w_id), d_id_(d_id), c_id_(c_id), item_info_(item_info) {}

bool TPCCNewOrderTransaction::execute() {
    try {
        double total_order_amount = 0.0;

        ResourceId warehouse_res_id = getTPCCResourceId("WAREHOUSE", w_id_);
        if (!acquireLock(warehouse_res_id, LockMode::EXCLUSIVE)) {
            abort();
            return false;
        }

        ResourceId district_res_id = getTPCCResourceId("DISTRICT", w_id_, d_id_);
        if (!acquireLock(district_res_id, LockMode::EXCLUSIVE)) {
            abort();
            return false;
        }

        District& district = db_.getDistrict(d_id_, w_id_);
        int o_id = district.d_next_o_id;
        district.d_next_o_id++;

        ResourceId customer_res_id = getTPCCResourceId("CUSTOMER", w_id_, d_id_, c_id_);
        if (!acquireLock(customer_res_id, LockMode::SHARED)) {
            abort();
            return false;
        }

        Order new_order_entry;
        new_order_entry.o_id = o_id;
        new_order_entry.o_d_id = d_id_;
        new_order_entry.o_w_id = w_id_;
        new_order_entry.o_c_id = c_id_;
        new_order_entry.o_entry_d = rng_.getCurrentTimestamp();
        new_order_entry.o_ol_cnt = item_info_.size();
        new_order_entry.o_all_local = 1;

        ResourceId order_res_id = getTPCCResourceId("ORDER", w_id_, d_id_, 0, 0, o_id);
        if (!acquireLock(order_res_id, LockMode::EXCLUSIVE)) {
            abort(); return false;
        }

        ResourceId new_order_res_id = getTPCCResourceId("NEW_ORDER", w_id_, d_id_, 0, 0, o_id);
        if (!acquireLock(new_order_res_id, LockMode::EXCLUSIVE)) {
            abort(); return false;
        }

        db_.orders.push_back(new_order_entry);
        db_.new_orders.push_back({o_id, d_id_, w_id_});

        for (size_t i = 0; i < item_info_.size(); ++i) {
            int ol_i_id = item_info_[i].first;
            // int ol_supply_w_id = item_info_[i].second;
            int ol_quantity = 5;

            int ol_supply_w_id;
            if (rng_.generateRandomDouble(0.0, 1.0) < 0.2) {
                ol_supply_w_id = rng_.generateRandomWarehouseId(home_node_id_);
            } else {
                ol_supply_w_id = w_id_;
            }

            ResourceId item_res_id = getTPCCResourceId("ITEM", 0, 0, 0, ol_i_id);
            if (!acquireLock(item_res_id, LockMode::SHARED)) {
                abort(); return false;
            }

            ResourceId stock_res_id = getTPCCResourceId("STOCK", ol_supply_w_id, 0, 0, ol_i_id);
            if (!acquireLock(stock_res_id, LockMode::EXCLUSIVE)) {
                abort(); return false;
            }

            Stock& stock = db_.getStock(ol_i_id, ol_supply_w_id);
            stock.s_quantity -= ol_quantity;
            if (stock.s_quantity < 10) {
                stock.s_quantity += 100;
            }
            stock.s_ytd += ol_quantity;
            stock.s_order_cnt++;

            OrderLine ol;
            ol.ol_o_id = o_id;
            ol.ol_d_id = d_id_;
            ol.ol_w_id = w_id_;
            ol.ol_number = i + 1;
            ol.ol_i_id = ol_i_id;
            ol.ol_supply_w_id = ol_supply_w_id;
            ol.ol_quantity = ol_quantity;
            ol.ol_amount = 10.0;
            ol.ol_dist_info = "some_dist_info";
            total_order_amount += ol.ol_amount;

            ResourceId order_line_res_id = getTPCCResourceId("ORDER_LINE", w_id_, d_id_, 0, 0, o_id, ol.ol_number);
            if (!acquireLock(order_line_res_id, LockMode::EXCLUSIVE)) {
                abort(); return false;
            }

            db_.order_lines.push_back(ol);
        }

        Warehouse& warehouse = db_.getWarehouse(w_id_);
        warehouse.w_ytd += total_order_amount;

        releaseAllLocks();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Trans " << id << ": New-Order 事务失败: " << e.what() << "\n";
        abort();
        return false;
    }
}

TPCCPaymentTransaction::TPCCPaymentTransaction(TPCCDatabase& db, LockTable& lock_table,
                                               TransactionId txn_id, NodeId home_node_id, TPCCRandom& rng,
                                               int w_id, int d_id, int c_w_id, int c_d_id, int c_id, double h_amount)
    : TPCCTransaction(db, lock_table, txn_id, home_node_id, rng),
      w_id_(w_id), d_id_(d_id), c_w_id_(c_w_id), c_d_id_(c_d_id), c_id_(c_id), h_amount_(h_amount), by_last_name_(false) {}

bool TPCCPaymentTransaction::execute() {
    try {
        ResourceId warehouse_res_id = getTPCCResourceId("WAREHOUSE", w_id_);
        if (!acquireLock(warehouse_res_id, LockMode::EXCLUSIVE)) {
            abort(); return false;
        }

        Warehouse& warehouse = db_.getWarehouse(w_id_);
        warehouse.w_ytd += h_amount_;

        ResourceId district_res_id = getTPCCResourceId("DISTRICT", w_id_, d_id_);
        if (!acquireLock(district_res_id, LockMode::EXCLUSIVE)) {
            abort(); return false;
        }

        District& district = db_.getDistrict(d_id_, w_id_);
        district.d_ytd += h_amount_;

        ResourceId customer_res_id = getTPCCResourceId("CUSTOMER", c_w_id_, c_d_id_, c_id_);
        if (!acquireLock(customer_res_id, LockMode::EXCLUSIVE)) {
            abort(); return false;
        }

        Customer& customer = db_.getCustomer(c_id_, c_d_id_, c_w_id_);
        customer.c_balance -= h_amount_;
        customer.c_ytd_payment += h_amount_;
        customer.c_payment_cnt++;

        History history;
        history.h_c_id = c_id_;
        history.h_c_d_id = c_d_id_;
        history.h_c_w_id = c_w_id_;
        history.h_d_id = d_id_;
        history.h_w_id = w_id_;
        history.h_date = rng_.getCurrentTimestamp();
        history.h_amount = h_amount_;
        history.h_data = "some_history_data";

        ResourceId history_res_id = getTPCCResourceId("HISTORY", w_id_, d_id_, c_id_);
        if (!acquireLock(history_res_id, LockMode::EXCLUSIVE)) {
            abort(); return false;
        }

        db_.histories.push_back(history);

        releaseAllLocks();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Trans " << id << ": Payment 事务失败: " << e.what() << "\n";
        abort();
        return false;
    }
}

TPCCOrderStatusTransaction::TPCCOrderStatusTransaction(TPCCDatabase& db, LockTable& lock_table,
                                                       TransactionId txn_id, NodeId home_node_id, TPCCRandom& rng,
                                                       int w_id, int d_id, int c_id)
    : TPCCTransaction(db, lock_table, txn_id, home_node_id, rng),
      w_id_(w_id), d_id_(d_id), c_id_(c_id), by_last_name_(false) {}

bool TPCCOrderStatusTransaction::execute() {
    try {
        ResourceId customer_res_id = getTPCCResourceId("CUSTOMER", w_id_, d_id_, c_id_);
        if (!acquireLock(customer_res_id, LockMode::SHARED)) {
            abort(); return false;
        }

        Order* last_order = nullptr;
        for (auto& order : db_.orders) {
            if (order.o_w_id == w_id_ && order.o_d_id == d_id_ && order.o_c_id == c_id_) {
                if (!last_order || order.o_id > last_order->o_id) {
                    last_order = &order;
                }
            }
        }

        if (!last_order) {
            releaseAllLocks();
            return true;
        }

        ResourceId order_res_id = getTPCCResourceId("ORDER", last_order->o_w_id, last_order->o_d_id, 0, 0, last_order->o_id);
        if (!acquireLock(order_res_id, LockMode::SHARED)) {
            abort(); return false;
        }

        for (auto& ol : db_.order_lines) {
            if (ol.ol_o_id == last_order->o_id && ol.ol_d_id == last_order->o_d_id && ol.ol_w_id == last_order->o_w_id) {
                ResourceId order_line_res_id = getTPCCResourceId("ORDER_LINE", ol.ol_w_id, ol.ol_d_id, 0, 0, ol.ol_o_id, ol.ol_number);
                if (!acquireLock(order_line_res_id, LockMode::SHARED)) {
                    abort(); return false;
                }
            }
        }

        releaseAllLocks();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Trans " << id << ": Order-Status 事务失败: " << e.what() << "\n";
        abort();
        return false;
    }
}

TPCCDeliveryTransaction::TPCCDeliveryTransaction(TPCCDatabase& db, LockTable& lock_table,
                                                 TransactionId txn_id, NodeId home_node_id, TPCCRandom& rng,
                                                 int w_id, int o_carrier_id)
    : TPCCTransaction(db, lock_table, txn_id, home_node_id, rng),
      w_id_(w_id), o_carrier_id_(o_carrier_id) {}

bool TPCCDeliveryTransaction::execute() {
    try {
        for (int d_id = 1; d_id <= 10; ++d_id) {
            NewOrder* oldest_new_order = nullptr;
            for (auto& no : db_.new_orders) {
                if (no.no_w_id == w_id_ && no.no_d_id == d_id) {
                    if (!oldest_new_order || no.no_o_id < oldest_new_order->no_o_id) {
                        oldest_new_order = &no;
                    }
                }
            }

            if (!oldest_new_order) {
                continue;
            }

            ResourceId new_order_res_id = getTPCCResourceId("NEW_ORDER", oldest_new_order->no_w_id, oldest_new_order->no_d_id, 0, 0, oldest_new_order->no_o_id);
            if (!acquireLock(new_order_res_id, LockMode::EXCLUSIVE)) {
                abort(); return false;
            }

            ResourceId order_res_id = getTPCCResourceId("ORDER", oldest_new_order->no_w_id, oldest_new_order->no_d_id, 0, 0, oldest_new_order->no_o_id);
            if (!acquireLock(order_res_id, LockMode::EXCLUSIVE)) {
                abort(); return false;
            }

            Order* order = nullptr;
            for (auto& o : db_.orders) {
                if (o.o_id == oldest_new_order->no_o_id && o.o_d_id == oldest_new_order->no_d_id && o.o_w_id == oldest_new_order->no_w_id) {
                    order = &o;
                    break;
                }
            }

            if (order) {
                order->o_carrier_id = o_carrier_id_;
            }

            double total_amount = 0;
            for (auto& ol : db_.order_lines) {
                if (ol.ol_o_id == oldest_new_order->no_o_id && ol.ol_d_id == oldest_new_order->no_d_id && ol.ol_w_id == oldest_new_order->no_w_id) {
                    ResourceId order_line_res_id = getTPCCResourceId("ORDER_LINE", ol.ol_w_id, ol.ol_d_id, 0, 0, ol.ol_o_id, ol.ol_number);
                    if (!acquireLock(order_line_res_id, LockMode::EXCLUSIVE)) {
                        abort(); return false;
                    }

                    ol.ol_delivery_d = rng_.getCurrentTimestamp();
                    total_amount += ol.ol_amount;
                }
            }

            if (order) {
                ResourceId customer_res_id = getTPCCResourceId("CUSTOMER", order->o_w_id, order->o_d_id, order->o_c_id);
                if (!acquireLock(customer_res_id, LockMode::EXCLUSIVE)) {
                    abort(); return false;
                }

                Customer& customer = db_.getCustomer(order->o_c_id, order->o_d_id, order->o_w_id);
                customer.c_balance += total_amount;
                customer.c_delivery_cnt++;
            }
        }

        releaseAllLocks();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Trans " << id << ": Delivery 事务失败: " << e.what() << "\n";
        abort();
        return false;
    }
}

TPCCStockLevelTransaction::TPCCStockLevelTransaction(TPCCDatabase& db, LockTable& lock_table,
                                                     TransactionId txn_id, NodeId home_node_id, TPCCRandom& rng,
                                                     int w_id, int d_id, int threshold)
    : TPCCTransaction(db, lock_table, txn_id, home_node_id, rng),
      w_id_(w_id), d_id_(d_id), threshold_(threshold) {}

bool TPCCStockLevelTransaction::execute() {
    try {
        ResourceId district_res_id = getTPCCResourceId("DISTRICT", w_id_, d_id_);
        if (!acquireLock(district_res_id, LockMode::SHARED)) {
            abort(); return false;
        }

        District& district = db_.getDistrict(d_id_, w_id_);

        std::vector<int> last_20_o_ids;
        std::vector<Order*> district_orders;
        for (auto& order : db_.orders) {
            if (order.o_w_id == w_id_ && order.o_d_id == d_id_) {
                district_orders.push_back(&order);
            }
        }

        std::sort(district_orders.begin(), district_orders.end(), [](const Order* a, const Order* b){
            return a->o_id < b->o_id;
        });

        int count = 0;
        for (auto it = district_orders.rbegin(); it != district_orders.rend() && count < 20; ++it, ++count) {
            last_20_o_ids.push_back((*it)->o_id);
        }

        std::unordered_set<int> low_stock_items;

        for (int o_id : last_20_o_ids) {
            for (const auto& ol : db_.order_lines) {
                if (ol.ol_o_id == o_id && ol.ol_d_id == d_id_ && ol.ol_w_id == w_id_) {
                    ResourceId stock_res_id = getTPCCResourceId("STOCK", ol.ol_supply_w_id, 0, 0, ol.ol_i_id);
                    if (!acquireLock(stock_res_id, LockMode::SHARED)) {
                        abort(); return false;
                    }

                    Stock& stock = db_.getStock(ol.ol_i_id, ol.ol_supply_w_id);
                    if (stock.s_quantity < threshold_) {
                        low_stock_items.insert(ol.ol_i_id);
                    }
                }
            }
        }

        releaseAllLocks();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Trans " << id << ": Stock-Level 事务失败: " << e.what() << "\n";
        abort();
        return false;
    }
}