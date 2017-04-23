#pragma once

#include <memory>
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
        size_t rows = 0;

        double rotate = 0;
        std::unique_ptr<SplitTree> left;
        std::unique_ptr<SplitTree> right;
    };
    SplitTree root_tree;
    std::array<size_t, MAX> mask;
    size_t depth = 0;

    struct Info
    {
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
        //std::cout << "entropy for " << positive << "/" << total - positive << " = " << e << std::endl;
        return e;
    }

    std::vector<double> get_unique(size_t pos, const List& fv)
    {
        std::vector<double> res;
        res.reserve(fv.size());
        for (auto& x : fv)
            res.push_back(x.second[pos]);
        std::sort(res.begin(), res.end());
        res.erase(std::unique(res.begin(), res.end()), res.end());
        return res;
    }

    Info collect_info(size_t pos, double val, const List& fv, bool eq = true)
    {
        Info i;
        for (auto& x : fv)
        {
            if ((eq and x.second[pos] == val))
            {
                i.total++;
                i.positive += x.first;
            }
            if (!eq and x.second[pos] <= val)
            {
                i.positive += x.first;
                i.total++;
            }
        }
        assert (i.total > 0);
        i.entropy = entropy(i.positive/(double)i.total);
        //std::cout << "entropy for (" << pos << ":" << val << ") " << i.positive << "/" << i.total - i.positive << " is " << i.entropy << std::endl;
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

    List make_partial(size_t pos, double value, const List& fv, bool less = true)
    {
        List res;
        res.reserve(fv.size());
        for (auto& x : fv)
            if ((less and x.second[pos] <= value) || (!less and x.second[pos] > value))
                res.push_back(x);
        return res;
    }

    void split(SplitTree& tree, size_t pos, const std::vector<double>& uni, const List& fv)
    {
        tree.attr_id = pos;
        std::vector<double> ents;

        double best_val = 0;
        double best_gain = 0;
        for (auto& x : uni)
        {
            auto fv_set1 = make_partial(pos, x, fv);
            auto fv_set2 = make_partial(pos, x, fv, false);
            auto ent = fv_set1.size() / (double)fv.size() * entropy(fv_set1) +
                       fv_set2.size() / (double)fv.size() * entropy(fv_set2);
            auto gain = entropy(fv) - ent;

            std::cout << "try value " << x << ", gain: " << gain << "(ent: " << ent << ")" << std::endl;
            if (fv_set1.size() > 0 and gain > best_gain) {
                best_gain = gain;
                best_val = x;
            }
        }

        if (best_gain == 0) {
            std::cout << "finished" << std::endl;
            tree.end = true;
            if (uni.empty())
                std::cout << "XXX empty unique set" << std::endl;
            else
                tree.value = get_result(pos, uni[0], fv);
            return;
        }
        std::cout << "best split on value " << best_val << " with gain " << best_gain << std::endl;

        tree.rotate = best_val;
        auto fv_set = make_partial(pos, best_val, fv);


        if (!fv_set.empty())
        {
            tree.left = std::make_unique<SplitTree>();
            if (fv_set.size() == fv.size())
            {
                std::cout << "XXX: set not decreasing" << std::endl;
                tree.left->end = true;
                tree.left->value = 0.5;
            } else {
                double ent = entropy(fv_set);
                if (0 == ent) {
                    std::cout << "entropy 0 after partitioning" << std::endl;
                    tree.left->end = true;
                    tree.left->value = fv_set[0].first;
                } else {
                    train(*tree.left, fv_set);
                }
            }
        }
        fv_set = make_partial(pos, best_val, fv, false);

        if (!fv_set.empty())
        {
            tree.right = std::make_unique<SplitTree>();
            if (fv_set.size() == fv.size())
            {
                std::cout << "XXX: set not decreasing" << std::endl;
                tree.right->end = true;
                tree.right->value = 0.5;
            } else {
                double ent = entropy(fv_set);
                if (0 == ent) {
                    std::cout << "entropy 0 after partitioning" << std::endl;
                    tree.right->end = true;
                    tree.right->value = fv_set[0].first;
                } else {
                    train(*tree.right, fv_set);
                }
            }
        }
    }

    void train(SplitTree& tree, const List& fv)
    {
        tree.rows = fv.size();
        double ent = entropy(fv);
        std::cout << "train step over " << fv.size() << " rows, initial entropy: " << ent << std::endl;
        // if number of rows not decreasing - break iteration

        size_t best_attr = 0;
        double best_gain = 0;
        std::vector<double> uniq_values;
        bool once = false;

re:
        for (size_t i = 0; i < max_features; i++)   // every attr
        {
            // hmmm. Oblivious Tree ?
            if (mask[i] > 0)    // do not split second time on the same attr
                continue;

            auto uni = get_unique(i, fv);
            auto g = ent - find_sum(i, uni, fv);
            std::cout << "found " << uni.size() << " uniq elements for feature " << i << ", gain: " << g << std::endl;
            if (g > best_gain) {
                best_attr = i;
                best_gain = g;
                uniq_values = std::move(uni);
            }
        }
        //if (depth < 100 and 0 == best_gain and std::all_of(mask.begin(), mask.end(), [](auto v){ return v == 1; })) {
        if (depth < 20 and 0 == best_gain and !once) {
            std::cout << "Oblivious!" << std::endl;
            std::fill(mask.begin(), mask.end(), 0);
            once = true;
            goto re;
        }
        if (0 == best_gain)
        {
            std::cout << "XXX no split (depth: " << depth << ")" << std::endl;
            for (auto& x : fv) {
                std::cout << x.first << ": ";
                for (auto& y : x.second)
                    std::cout << y << ',';
                std::cout << std::endl;
            }
            tree.end = true;
            tree.value = 0.5;
            return;
        }

        auto local_mask = mask;
        std::cout << std::endl << "split on attribute " << best_attr << " with gain " << best_gain << std::endl << std::endl;
        mask[best_attr]++;
        depth++;
        split(tree, best_attr, uniq_values, fv);
        depth--;
        mask = local_mask;
    }

    void train(const List& fv)
    {
        std::fill(mask.begin(), mask.end(), 0);
        train(root_tree, fv);
    }

    double predict(const SplitTree& tree, const Vector& fv)
    {
        if (tree.end)
            return tree.value;

        auto key = fv[tree.attr_id];
        if (key <= tree.rotate)
        {
            if (tree.left)
                return predict(*tree.left, fv);
            std::cerr << "XXX miss value" << std::endl;
        }
        else
        {
            if (tree.right)
                return predict(*tree.right, fv);
            std::cerr << "XXX miss value" << std::endl;
        }
        return 0.5;
    }

    double predict(const Vector& fv)
    {
        return predict(root_tree, fv);
    }

    void print(const SplitTree& tree, int off = 0)
    {
        std::string skip(off, ' ');
        if (tree.end)
            std::cout << skip << "end with value " << tree.value << std::endl;
        else
        {
//            std::cout << skip << "tree with " << tree.rows << " rows" << std::endl;
            if (tree.left)
            {
                std::cout << skip << "if attribute " << tree.attr_id << " le than " << tree.rotate << std::endl;
                print(*tree.left, off+1);
            }
            if (tree.right)
            {
                std::cout << skip << "if attribute " << tree.attr_id << " gt than " << tree.rotate << std::endl;
                print(*tree.right, off+1);
            }
        }
    }

    void print()
    {
        print(root_tree);
    }
};
