#pragma once

#include <string>
#include <vector>
#include <chrono>

struct TPCCDatabase;

struct Warehouse {
    int w_id;
    std::string w_name;
    std::string w_street_1;
    std::string w_street_2;
    std::string w_city;
    std::string w_state;
    std::string w_zip;
    double w_tax;
    double w_ytd;
};

struct District {
    int d_id;
    int d_w_id;
    std::string d_name;
    std::string d_street_1;
    std::string d_street_2;
    std::string d_city;
    std::string d_state;
    std::string d_zip;
    double d_tax;
    double d_ytd;
    int d_next_o_id;
};

struct Customer {
    int c_id;
    int c_d_id;
    int c_w_id;
    std::string c_first;
    std::string c_middle;
    std::string c_last;
    std::string c_street_1;
    std::string c_street_2;
    std::string c_city;
    std::string c_state;
    std::string c_zip;
    std::string c_phone;
    long long c_since;
    std::string c_credit;
    double c_credit_lim;
    double c_discount;
    double c_balance;
    double c_ytd_payment;
    int c_payment_cnt;
    int c_delivery_cnt;
    std::string c_data;
};

struct History {
    int h_c_id;
    int h_c_d_id;
    int h_c_w_id;
    int h_d_id;
    int h_w_id;
    long long h_date;
    double h_amount;
    std::string h_data;
};

struct Order {
    int o_id;
    int o_d_id;
    int o_w_id;
    int o_c_id;
    long long o_entry_d;
    int o_carrier_id;
    int o_ol_cnt;
    int o_all_local;
};

struct NewOrder {
    int no_o_id;
    int no_d_id;
    int no_w_id;
};

struct OrderLine {
    int ol_o_id;
    int ol_d_id;
    int ol_w_id;
    int ol_number;
    int ol_i_id;
    int ol_supply_w_id;
    int ol_quantity;
    double ol_amount;
    long long ol_delivery_d;
    std::string ol_dist_info;
};

struct Item {
    int i_id;
    int i_im_id;
    std::string i_name;
    double i_price;
    std::string i_data;
};

struct Stock {
    int s_i_id;
    int s_w_id;
    int s_quantity;
    std::string s_dist_01;
    std::string s_dist_02;
    std::string s_dist_03;
    std::string s_dist_04;
    std::string s_dist_05;
    std::string s_dist_06;
    std::string s_dist_07;
    std::string s_dist_08;
    std::string s_dist_09;
    std::string s_dist_10;
    int s_ytd;
    int s_order_cnt;
    int s_remote_cnt;
    std::string s_data;
};

struct TPCCDatabase {
    std::vector<Warehouse> warehouses;
    std::vector<District> districts;
    std::vector<Customer> customers;
    std::vector<History> histories;
    std::vector<Order> orders;
    std::vector<NewOrder> new_orders;
    std::vector<OrderLine> order_lines;
    std::vector<Item> items;
    std::vector<Stock> stocks;

    Warehouse& getWarehouse(int w_id) {
        return warehouses[w_id - 1];
    }

    District& getDistrict(int d_id, int w_id) {
        return districts[(w_id - 1) * 10 + (d_id - 1)];
    }

    Customer& getCustomer(int c_id, int d_id, int w_id) {
        return customers[(w_id - 1) * 10 * 3000 + (d_id - 1) * 3000 + (c_id - 1)];
    }

    Item& getItem(int i_id) {
        return items[i_id - 1];
    }

    Stock& getStock(int i_id, int w_id) {
        return stocks[(w_id - 1) * 100000 + (i_id - 1)];
    }
};