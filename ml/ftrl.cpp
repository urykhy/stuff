
#include <Ftrl.hpp>
#include <Parser.hpp>

void wine_test(size_t steps = 100)
{
    using FTRL = Ftrl<12>;
    FTRL wine;
    Parser<FTRL> loader;
    auto d = loader("wine.csv", "White");

    std::cout << "readed " << d.size() << " elements" << std::endl;

    size_t split_at = d.size() * 0.75;
    FTRL::List d_train(d.begin(), d.begin() + split_at);
    FTRL::List d_test(d.begin() + split_at, d.end());
    d.clear();

    std::cout << "train on " << d_train.size() << " elements" << std::endl;
    for (size_t i = 0; i < steps; i++)
    {
        wine.train(d_train);
        std::cout << "last error: " << wine.calc_error(d_train) << std::endl;
    }

    size_t ok = 0;
    for (const auto& x : d_test) {
        const auto p = wine.predict(x.second);
        if (x.first == false && p < 0.5) ok++;
        if (x.first == true  && p > 0.5) ok++;
        //std::cout << p << " ";
    }
    std::cout << "OK is " << ok << "(" << ok / (double)d_test.size() * 100.<< "%)" << std::endl;
}

int main(void)
{
#if 0
    Ftrl<3> model;

    const std::array<typename Ftrl<3>::Entry, 4> d{{
        {true,  {0,0,1}},
        {true,  {0,1,1}},
        {false, {1,0,1}},
        {false, {1,1,1}}
    }};

    for (size_t i = 0; i < 10; i++)
    {
        for (const auto& x d) model.train(x.first, x.second);
        std::cout << "last log loss " << model.last_logloss() << std::endl;
    }
    std::cout << model.test({0,0,1}) << std::endl;
    std::cout << model.test({0,1,1}) << std::endl;
    std::cout << model.test({1,0,1}) << std::endl;
    std::cout << model.test({1,1,1}) << std::endl;

#else
    wine_test();
#endif

    return 0;
}

