#include "RandomGenerators.h"
#include <algorithm>
#include <random>

std::mt19937& RandomGenerators::getEngine()
{
    static std::mt19937 gen(std::random_device{}());
    return gen;
}

int RandomGenerators::getRandomInt(int min, int max)
{
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(getEngine());
}

int RandomGenerators::getExponentialInt(double lambda, int min_val, int max_val)
{
    std::exponential_distribution<> distrib(lambda);
    int val = static_cast<int>(distrib(getEngine()));
    return std::min(std::max(val, min_val), max_val);
}

double RandomGenerators::getRandomDouble(double min, double max)
{
    std::uniform_real_distribution<> distrib(min, max);
    return distrib(getEngine());
}