#pragma once

namespace Container {

    template <class T, class Predicate>
    void discard_if(T& c, Predicate pred)
    {
        for (auto it{c.begin()}, end{c.end()}; it != end;) {
            if (pred(*it))
                it = c.erase(it);
            else
                ++it;
        }
    }

    // https://stackoverflow.com/questions/53205898/how-do-i-merge-two-maps-in-stl-and-apply-a-function-for-conflicts/53206267#53206267
    template <class Map, class Merger>
    void merge(Map& dest, const Map& source, Merger merger)
    {
        auto   it1  = dest.begin();
        auto   it2  = source.begin();
        auto&& comp = dest.value_comp();

        for (; it1 != dest.end() && it2 != source.end();) {
            if (comp(*it1, *it2)) {
                ++it1;
            } else if (comp(*it2, *it1)) {
                dest.insert(it1, *it2);
                ++it2;
            } else { // equivalent
                it1->second = merger(it1->second, it2->second);
                ++it1;
                ++it2;
            }
        }
        dest.insert(it2, source.end());
    }
} // namespace Container
