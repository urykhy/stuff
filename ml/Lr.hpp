
#pragma once

#include <array>
#include <cmath>
#include <random>

template<int MAX>
struct LR
{
    const double alpha = 5;

    using Vector = std::array<double, MAX>;
    using Entry = std::pair<bool, Vector>;
    using List = std::vector<Entry>;
    enum {max_features = MAX};
    enum {initial_one = 1}; // 1st element in feature vector must be 1

    Vector w;

    LR()
    {
        // init random weights
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0, 1);

        for (auto& x : w) {
            x = dis(gen);
        }
    }

    double sigmoid(const double a)
    {
        return 1. / (1. + std::exp(-a));
    }

    double logloss(bool y, const Vector& fv)
    {
        const double p = predict(fv);
        return -std::log( y ? p : (1 - p) );
    }

    double predict(const Vector& fv)
    {
        double res = 0;
        for (size_t i = 0; i < fv.size(); i++)
        {
            res += w[i] * fv[i];
        }
        return sigmoid(res);
    }

    // GD
    template<class T>
    Vector get_df(const T& training)
    {
        std::vector<double> pred;
        pred.resize(training.size());
        for (size_t i = 0; i < training.size(); i++) {
            pred[i] = predict(training[i].second);
        }

        // one step
        Vector nw;
        for (size_t j = 0; j < max_features; j++)
        {
            double r = 0;
            for (size_t i = 0; i < training.size(); i++) {
                r += (pred[i] - (training[i].first ? 1. : 0.)) * training[i].second[j];
            };
            r /= training.size();
            nw[j] = r;
        }
        return nw;
    }

    template<class T>
    void train(const T& training)
    {
        const auto nw = get_df(training);
        for (size_t i = 0; i < max_features; i++) {
            w[i] -= alpha * nw[i];
        }
    }

    template<class T>
    double calc_error(const T& training)
    {
        double c = 0;
        for (size_t i = 0; i < training.size(); i++)
        {
            c += logloss(training[i].first, training[i].second);
        }
        c = c / double(training.size());
        return c;
    }
};

