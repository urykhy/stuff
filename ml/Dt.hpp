#pragma once

#include <vector>
#include <map>
#include <iomanip>

template<int MAX>
struct Dt
{
    // Parser API
    using Vector = std::array<double, MAX>;
    using Entry = std::pair<bool, Vector>;
    using List = std::vector<Entry>;
    enum {max_features = MAX};
    enum {initial_one = 0};
    //

    struct SplitTree;
    using Split = std::map<double, SplitTree>;  // value - > child
    struct SplitTree {
        size_t attr_id = 0xFFFF;
        bool end = false;
        bool value = false;
        Split child;
    };
    SplitTree root_tree;
    Vector mask;

    struct Info
    {
        bool result = 0;
        size_t total = 0;
        size_t positive = 0;
        double entropy = 0;
    };

    // proportion of positive elements
    double entropy(double pos)
    {
        if (pos == 0 or pos == 1)
            return 0;
        return -pos * std::log2(pos) - (1 - pos) * std::log2(1-pos);
    }

    double entropy(const List& fv)
    {
        size_t total = 0;
        size_t positive = 0;
        for (auto& x : fv)
        {
            total++;
            positive += x.first;
        }
        auto e = entropy(positive/(double)total);
        std::cout << "entropy for " << positive << "/" << total - positive << " = " << e << std::endl;
        return e;
    }

    std::vector<double> make_unique(size_t pos, const List& fv)
    {
        std::vector<double> res;
        res.reserve(fv.size());
        for (auto& x : fv)
            res.push_back(x.second[pos]);
        std::sort(res.begin(), res.end());
        res.erase(std::unique(res.begin(), res.end()), res.end());
        return res;
    }

    Info collect_info(size_t pos, double val, const List& fv)
    {
        Info i;
        for (auto& x : fv)
        {
            if (x.second[pos] == val)
            {
                i.total++;
                i.positive += x.first;
                i.result = x.first;
            }
        }
        assert (i.total > 0);
        i.entropy = entropy(i.positive/(double)i.total);
        std::cout << "entropy for (" << pos << ":" << val << ") " << i.positive << "/" << i.total - i.positive << " is " << i.entropy << std::endl;
        return i;
    }

    double find_sum(size_t pos, const std::vector<double>& uni, const List& fv)
    {
        double sum = 0;
        for (auto& x : uni)
        {
            auto info = collect_info(pos, x, fv);
            //std::cout << "sum " << info.total << " / " << fv.size() << " * " << info.entropy << std::endl;
            sum += info.total / (double)fv.size() * info.entropy;
        }
        return sum;
    }

    bool get_result(size_t pos, double val, const List& fv)
    {
        for (auto& x : fv)
            if (x.second[pos] == val)
                return x.first;
        throw "1";
    }

    List make_partial(size_t pos, double value, const List& fv)
    {
        List res;
        res.reserve(fv.size());
        for (auto& x : fv)
            if (x.second[pos] == value)
                res.push_back(x);
        return res;
    }

    void fill(SplitTree& tree, size_t pos, const std::vector<double>& uni, const List& fv)
    {
        tree.attr_id = pos;
        for (auto& x : uni)
        {
            std::cout << "split value " << x << std::endl;
            auto part_fv = make_partial(pos, x, fv);
            double ent = entropy(part_fv);
            auto& leaf = tree.child[x] = SplitTree();
            if (0 == ent)
            {
                std::cout << "finished" << std::endl;
                leaf.end = true;
                leaf.value = get_result(pos, x, fv);
            } else {
                std::cout << "want to train " << std::endl;
                train(leaf, part_fv);
            }
        }
    }

    void train(SplitTree& tree, const List& fv)
    {
        double ent = entropy(fv);
        std::cout << "train step, initial entropy: " << ent << std::endl;

        size_t best_attr = 0;
        double best_gain = 0;
        std::vector<double> uniq_values;
        for (size_t i = 0; i < max_features; i++)   // every attr
        {
            if (mask[i])    // do not split second time on the same attr
                continue;

            auto uni = make_unique(i, fv);
            auto g = ent - find_sum(i, uni, fv);
            std::cout << "found " << uni.size() << " uniq elements for feature " << i << ", gain: " << std::setprecision(10) << g << std::endl;
            if (g > best_gain) {
                best_attr = i;
                best_gain = g;
                uniq_values = std::move(uni);
            }
        }
        if (0 == best_gain)
        {
            std::cout << "no split" << std::endl;
            tree.end = true;
            return;
        }
        std::cout << std::endl << "split on " << best_attr << " with gain " << best_gain << std::endl << std::endl;

        Vector local_mask = mask;
        mask[best_attr] = 1;
        fill(tree, best_attr, uniq_values, fv);
        mask = local_mask;
    }

    void train(const List& fv)
    {
        std::fill(mask.begin(), mask.end(), 0);
        train(root_tree, fv);
    }

    double predict(const SplitTree& tree, const Vector& fv)
    {
        auto key = fv[tree.attr_id];
        auto it = tree.child.find(key);

        if (it == tree.child.end())
        {
            std::cout << "no value " << key << std::endl;
            return 0.5;  // no value
        }

        auto& res = it->second;
        if (res.end) {
            return res.value;
        }
        return predict(res, fv);
    }

    double predict(const Vector& fv)
    {
        return predict(root_tree, fv);
    }

};
