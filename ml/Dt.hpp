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
    struct SplitTree {
        size_t attr_id = 0;
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
        return e;
    }

    // entropy of chunk
    auto entropy(size_t pos, const List& fv, bool le, double rotate)
    {
        size_t total = 0;
        size_t positive = 0;
        for (auto& x : fv)
        {
            if ((le and x.second[pos] <= rotate) or (!le and x.second[pos] > rotate))
            {
                total++;
                positive += x.first;
            }
        }
        auto e = entropy(positive/(double)total);
        return std::make_pair(total, e);
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

    auto calc_split(size_t pos, const std::vector<double>& uni, const List& fv)
    {
        double best_ent = 1;
        double rotate_val = 0;
        for (auto& x : uni)
        {
            auto i1 = entropy(pos, fv, false, x);
            auto i2 = entropy(pos, fv, true, x);
            auto ent = i1.first / (double)fv.size() * i1.second +
                       i2.first / (double)fv.size() * i2.second;
            if ((i1.first > 0 and ent < best_ent) or best_ent == 1) {
                best_ent = ent;
                rotate_val = x;
            }
        }
        return std::make_pair(best_ent, rotate_val);
    }

    // FIXME: make some kind of view-class
    List make_partial(size_t pos, double value, const List& fv, bool less = true)
    {
        List res;
        res.reserve(fv.size());
        for (auto& x : fv)
            if ((less and x.second[pos] <= value) || (!less and x.second[pos] > value))
                res.push_back(x);
        return res;
    }

    void train_one(SplitTree& tree, const List& fv_set, const List& fv)
    {
        assert (fv_set.size() < fv.size());
        double ent = entropy(fv_set);
        if (0 == ent) {
            std::cout << "entropy 0 after partitioning" << std::endl;
            tree.end = true;
            tree.value = fv_set[0].first;
        } else {
            train(tree, fv_set);
        }
    }

    void split(SplitTree& tree, size_t pos, const double rotate, const List& fv)
    {
        tree.attr_id = pos;
        tree.rotate = rotate;
        std::vector<double> ents;

        auto fv_set = make_partial(pos, rotate, fv);

        if (!fv_set.empty())
        {
            tree.left = std::make_unique<SplitTree>();
            train_one(*tree.left, fv_set, fv);
        }
        fv_set = make_partial(pos, rotate, fv, false);

        if (!fv_set.empty())
        {
            tree.right = std::make_unique<SplitTree>();
            train_one(*tree.right, fv_set, fv);
        }
    }

    void train(SplitTree& tree, const List& fv)
    {
        tree.rows = fv.size();
        const double ent = entropy(fv);
        std::cout << "train step over " << fv.size() << " rows, initial entropy: " << ent << std::endl;
        // if number of rows not decreasing - break iteration

        size_t best_attr = 0;
        double best_gain = 0;
        double rotate = 0;
        bool once = false;

re:
        for (size_t i = 0; i < max_features; i++)   // every attr
        {
            // hmmm. Oblivious Tree ?
            if (mask[i] > 0)    // do not split second time on the same attr
                continue;

            auto uni = get_unique(i, fv);
            auto info = calc_split(i, uni, fv);
            auto g = ent - info.first;
            std::cout << "found " << uni.size() << " uniq elements for feature " << i << ", gain: " << g << std::endl;
            if (g > best_gain) {
                best_attr = i;
                best_gain = g;
                rotate = info.second;
            }
        }
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
        std::cout << std::endl << "split on attribute " << best_attr << ", value " << rotate<< " with gain " << best_gain << std::endl << std::endl;
        mask[best_attr]++;
        depth++;
        split(tree, best_attr, rotate, fv);
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
        }
        else
        {
            if (tree.right)
                return predict(*tree.right, fv);
        }
        assert(0);
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
