
#pragma once

#include <array>
#include <cmath>
#include <vector>

template<int MAX>
struct Ftrl
{
    const double alpha = 1.;
    const double beta = 1.;
    const int L1 = 1.0;
    const int L2 = 1.0;

    using Vector = std::array<double, MAX>;
    using Entry = std::pair<bool, Vector>;
    using List = std::vector<Entry>;
    enum {max_features = MAX};
    enum {initial_one = 0};

    Vector z;
    Vector n;
    Vector w;

    void init(Vector& v)
    {
        for (auto& x : v)
            x = 0;
    }

    Ftrl()
    {
        init(z);
        init(n);
        init(w);
    }

    double sigmoid(const double a)
    {
        return 1. / (1. + std::exp(-a));
    }

    double logloss(bool y, const Vector& fv)    // y is a target. true or false
    {
        const double p = predict(fv);
        return -std::log( y ? p : (1 - p) );
    }

    double predict(const Vector& fv)
    {
        double res = 0;
        for (size_t i = 0; i < fv.size(); i++)
        {
            if (std::abs(z[i]) <= L1) {
                w[i] = 0;
            } else {
                const double sign = z[i] >= 0 ? 1. : -1.;
                w[i] = - (z[i] - sign * L1) / (L2 + (beta + std::sqrt(n[i])) / alpha);
            }
            res += w[i] * fv[i];
        }
        return sigmoid(res);
    }

    // feature - is a index. value - is a weight ?
    void train(const bool y, const Vector& fv)
    {
        // predict
        const auto p = predict(fv);

        // update
        for (size_t i = 0; i < fv.size(); i++)
        {
            const auto g = (p - (y ? 1 : 0)) * fv[i];
            const auto s = (std::sqrt(n[i] + g * g) - std::sqrt(n[i])) / alpha;
            z[i] = z[i] + g - s * w[i];
            n[i] = n[i] + g * g;
        }
    }

    template<class T>
    void train(const T& fv)
    {
        for (const auto& x : fv) {
            train(x.first, x.second);
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


