#include "tpcc_data_generator.h"
#include <iostream>
#include <numeric>
#include <algorithm>

TPCCDatabase TPCCDataGenerator::generateData(int num_warehouses) {
    TPCCDatabase db;

    db.warehouses.resize(num_warehouses);
    db.districts.resize(num_warehouses * 10);
    db.customers.resize(num_warehouses * 10 * 3000);
    db.histories.reserve(num_warehouses * 10 * 3000);
    db.orders.reserve(num_warehouses * 10 * 3000);
    db.new_orders.reserve(num_warehouses * 10 * 900);
    db.order_lines.reserve(num_warehouses * 10 * 3000 * 15);
    db.items.resize(100000);
    db.stocks.resize(num_warehouses * 100000);

    std::cout << "Generating TPC-C data for " << num_warehouses << " warehouses...\n";

    generateItems(db);

    for (int w_id = 1; w_id <= num_warehouses; ++w_id) {
        generateWarehouse(db, w_id);
        generateStock(db, w_id);

        for (int d_id = 1; d_id <= 10; ++d_id) {
            generateDistrict(db, w_id, d_id);
            for (int c_id = 1; c_id <= 3000; ++c_id) {
                generateCustomer(db, w_id, d_id, c_id);
            }
            generateOrders(db, w_id, d_id);
        }
    }
    std::cout << "TPC-C data generation complete.\n";
    return db;
}

void TPCCDataGenerator::generateItems(TPCCDatabase& db) {
    std::cout << "Generating Items...\n";
    for (int i_id = 1; i_id <= 100000; ++i_id) {
        Item& item = db.items[i_id - 1];
        item.i_id = i_id;
        item.i_im_id = rng_.generateRandomInt(1, 10000);
        item.i_name = rng_.generateAString(14, 24);
        item.i_price = rng_.generateRandomDouble(1.00, 100.00);
        item.i_data = rng_.generateAString(26, 50);

        if (rng_.generateRandomInt(1, 10) == 1) {
            int original_pos = rng_.generateRandomInt(0, item.i_data.length() - 8);
            item.i_data.replace(original_pos, 8, "ORIGINAL");
        }
    }
}

void TPCCDataGenerator::generateWarehouse(TPCCDatabase& db, int w_id) {
    Warehouse& wh = db.warehouses[w_id - 1];
    wh.w_id = w_id;
    wh.w_name = rng_.generateAString(6, 10);
    wh.w_street_1 = generateStreet();
    wh.w_street_2 = generateStreet();
    wh.w_city = generateCity();
    wh.w_state = generateState();
    wh.w_zip = generateZip();
    wh.w_tax = rng_.generateRandomDouble(0.0000, 0.2000);
    wh.w_ytd = 300000.00;
}

void TPCCDataGenerator::generateDistrict(TPCCDatabase& db, int w_id, int d_id) {
    District& dist = db.getDistrict(d_id, w_id);
    dist.d_id = d_id;
    dist.d_w_id = w_id;
    dist.d_name = rng_.generateAString(6, 10);
    dist.d_street_1 = generateStreet();
    dist.d_street_2 = generateStreet();
    dist.d_city = generateCity();
    dist.d_state = generateState();
    dist.d_zip = generateZip();
    dist.d_tax = rng_.generateRandomDouble(0.0000, 0.2000);
    dist.d_ytd = 30000.00;
    dist.d_next_o_id = 3001;
}

void TPCCDataGenerator::generateCustomer(TPCCDatabase& db, int w_id, int d_id, int c_id) {
    Customer& cust = db.getCustomer(c_id, d_id, w_id);
    cust.c_id = c_id;
    cust.c_d_id = d_id;
    cust.c_w_id = w_id;
    cust.c_first = rng_.generateAString(8, 16);
    cust.c_middle = "OE";
    cust.c_last = rng_.generateLastName(c_id);
    cust.c_street_1 = generateStreet();
    cust.c_street_2 = generateStreet();
    cust.c_city = generateCity();
    cust.c_state = generateState();
    cust.c_zip = generateZip();
    cust.c_phone = rng_.generateNString(16, 16);
    cust.c_since = rng_.getCurrentTimestamp();
    cust.c_credit = (rng_.generateRandomInt(1, 100) > 90) ? "BC" : "GC";
    cust.c_credit_lim = 50000.00;
    cust.c_discount = rng_.generateRandomDouble(0.0000, 0.5000);
    cust.c_balance = -10.00;
    cust.c_ytd_payment = 10.00;
    cust.c_payment_cnt = 1;
    cust.c_delivery_cnt = 0;
    cust.c_data = rng_.generateAString(300, 500);

    History hist;
    hist.h_c_id = c_id;
    hist.h_c_d_id = d_id;
    hist.h_c_w_id = w_id;
    hist.h_d_id = d_id;
    hist.h_w_id = w_id;
    hist.h_date = rng_.getCurrentTimestamp();
    hist.h_amount = 10.00;
    hist.h_data = rng_.generateAString(10, 24);
    db.histories.push_back(hist);
}

void TPCCDataGenerator::generateStock(TPCCDatabase& db, int w_id) {
    for (int i_id = 1; i_id <= 100000; ++i_id) {
        Stock& stock = db.getStock(i_id, w_id);
        stock.s_i_id = i_id;
        stock.s_w_id = w_id;
        stock.s_quantity = rng_.generateRandomInt(10, 100);
        stock.s_dist_01 = rng_.generateAString(24, 24);
        stock.s_dist_02 = rng_.generateAString(24, 24);
        stock.s_dist_03 = rng_.generateAString(24, 24);
        stock.s_dist_04 = rng_.generateAString(24, 24);
        stock.s_dist_05 = rng_.generateAString(24, 24);
        stock.s_dist_06 = rng_.generateAString(24, 24);
        stock.s_dist_07 = rng_.generateAString(24, 24);
        stock.s_dist_08 = rng_.generateAString(24, 24);
        stock.s_dist_09 = rng_.generateAString(24, 24);
        stock.s_dist_10 = rng_.generateAString(24, 24);
        stock.s_ytd = 0;
        stock.s_order_cnt = 0;
        stock.s_remote_cnt = 0;
        stock.s_data = rng_.generateAString(26, 50);

        if (rng_.generateRandomInt(1, 10) == 1) {
            int original_pos = rng_.generateRandomInt(0, stock.s_data.length() - 8);
            stock.s_data.replace(original_pos, 8, "ORIGINAL");
        }
    }
}

void TPCCDataGenerator::generateOrders(TPCCDatabase& db, int w_id, int d_id) {
    std::vector<int> c_ids(3000);
    std::iota(c_ids.begin(), c_ids.end(), 1);
    std::shuffle(c_ids.begin(), c_ids.end(), rng_.getGen());

    for (int o_id = 1; o_id <= 3000; ++o_id) {
        Order order;
        order.o_id = o_id;
        order.o_d_id = d_id;
        order.o_w_id = w_id;
        order.o_c_id = c_ids[o_id - 1];
        order.o_entry_d = rng_.getCurrentTimestamp();
        order.o_ol_cnt = rng_.generateRandomInt(5, 15);
        order.o_all_local = 1;

        if (o_id <= 2100) {
            order.o_carrier_id = rng_.generateRandomInt(1, 10);
        } else {
            order.o_carrier_id = 0;
            NewOrder new_order;
            new_order.no_o_id = o_id;
            new_order.no_d_id = d_id;
            new_order.no_w_id = w_id;
            db.new_orders.push_back(new_order);
        }
        db.orders.push_back(order);

        for (int ol_number = 1; ol_number <= order.o_ol_cnt; ++ol_number) {
            OrderLine ol;
            ol.ol_o_id = o_id;
            ol.ol_d_id = d_id;
            ol.ol_w_id = w_id;
            ol.ol_number = ol_number;
            ol.ol_i_id = rng_.generateRandomInt(1, 100000);
            ol.ol_supply_w_id = w_id;
            ol.ol_quantity = 5;
            ol.ol_amount = rng_.generateRandomDouble(0.01, 9999.99);
            ol.ol_dist_info = rng_.generateAString(24, 24);

            if (o_id <= 2100) {
                ol.ol_delivery_d = rng_.getCurrentTimestamp();
            } else {
                ol.ol_delivery_d = 0;
            }
            db.order_lines.push_back(ol);
        }
    }
}

std::string TPCCDataGenerator::generateStreet() {
    return rng_.generateAString(10, 20);
}

std::string TPCCDataGenerator::generateCity() {
    return rng_.generateAString(10, 20);
}

std::string TPCCDataGenerator::generateState() {
    return rng_.generateAString(2, 2);
}

std::string TPCCDataGenerator::generateZip() {
    return rng_.generateNString(4, 4) + "11111";
}