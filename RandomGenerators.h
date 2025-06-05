#ifndef HAWK_RANDOM_GENERATORS_H
#define HAWK_RANDOM_GENERATORS_H

#include <random>
#include <algorithm>

class RandomGenerators
{
public:
    static std::mt19937& getEngine();

    static int getRandomInt(int min, int max);

    static int getExponentialInt(double lambda, int min_val, int max_val);

    static double getRandomDouble(double min, double max);
};

#endif // HAWK_RANDOM_GENERATORS_H