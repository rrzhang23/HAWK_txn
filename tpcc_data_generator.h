#pragma once

#include "tpcc.h"
#include "commons.h"
#include <random>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>

class TPCCRandom {
public:
    TPCCRandom() : gen_(std::random_device{}()) {}

    std::string generateAString(int min_len, int max_len) {
        int length = generateRandomInt(min_len, max_len);
        std::string s(length, ' ');
        static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        for (int i = 0; i < length; ++i) {
            s[i] = alphanum[generateRandomInt(0, sizeof(alphanum) - 2)];
        }
        return s;
    }

    std::string generateNString(int min_len, int max_len) {
        int length = generateRandomInt(min_len, max_len);
        std::string s(length, ' ');
        static const char num[] = "0123456789";
        for (int i = 0; i < length; ++i) {
            s[i] = num[generateRandomInt(0, sizeof(num) - 2)];
        }
        return s;
    }

    int generateRandomInt(int min, int max) {
        return std::uniform_int_distribution<int>(min, max)(gen_);
    }

    double generateRandomDouble(double min, double max) {
        return std::uniform_real_distribution<double>(min, max)(gen_);
    }

    int NURand(int A, int x, int y) {
        return (((generateRandomInt(0, A) | generateRandomInt(x, y)) + C_LAST) % (y - x + 1)) + x;
    }

    int generateCID() {
        return NURand(1023, 1, 3000);
    }

    int generateItemID() {
        return NURand(8191, 1, 100000);
    }

    std::string generateLastName(int c_id) {
        static const std::vector<std::string> SYLLABLES = {
            "BAR", "OUGHT", "ABLE", "PRI", "PRES", "ESE", "ANTI", "CALLY", "ATION", "EING"
        };
        std::string last_name = "";
        int num = c_id % 1000;
        last_name += SYLLABLES[num / 100];
        last_name += SYLLABLES[(num / 10) % 10];
        last_name += SYLLABLES[num % 10];
        return last_name;
    }

    long long getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    std::mt19937& getGen() {
        return gen_;
    }

    
    inline int generateRandomWarehouseId(NodeId home_node_id) {
        int home_warehouse_start_id = (home_node_id / NUM_DOMAINS) * WAREHOUSES_PER_NODE + 1;
        int home_warehouse_end_id = home_warehouse_start_id + WAREHOUSES_PER_NODE * NUM_DOMAINS - 1;

        if (generateRandomDouble(0.0, 1.0) < DOMAIN_LOCAL_ACCESS_PROBABILITY) {
            // 80% local zone access
            return generateRandomInt(home_warehouse_start_id, home_warehouse_end_id);
        } else {
            // 20% cross zone access
            int target_w_id;
            do {
                target_w_id = generateRandomInt(1, NUM_WAREHOUSES);
            } while (target_w_id >= home_warehouse_start_id && target_w_id <= home_warehouse_end_id);

            return target_w_id;
        }
    }

private:
    std::mt19937 gen_;
    static const int C_LAST = 255;
    static const int C_ID = 1023;
    static const int OL_I_ID = 8191;
};

class TPCCDataGenerator {
public:
    TPCCDatabase generateData(int num_warehouses);

private:
    TPCCRandom rng_;

    void generateItems(TPCCDatabase& db);
    void generateWarehouse(TPCCDatabase& db, int w_id);
    void generateDistrict(TPCCDatabase& db, int w_id, int d_id);
    void generateCustomer(TPCCDatabase& db, int w_id, int d_id, int c_id);
    void generateStock(TPCCDatabase& db, int w_id);
    void generateOrders(TPCCDatabase& db, int w_id, int d_id);

    std::string generateStreet();
    std::string generateCity();
    std::string generateState();
    std::string generateZip();
};