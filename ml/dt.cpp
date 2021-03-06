
#include <Parser.hpp>
#include <Dt.hpp>

void wine_test()
{
    using DT = Dt<12>;
    DT wine;
    Parser<DT> loader;
    auto d = loader("wine.csv", "White");

    std::cout << "readed " << d.size() << " elements" << std::endl;

    size_t split_at = d.size() * 0.75;
    DT::List d_train(d.begin(), d.begin() + split_at);
    DT::List d_test(d.begin() + split_at, d.end());
    d.clear();

    std::cout << "train on " << d_train.size() << " elements" << std::endl;

    for (size_t i = 0; i < 10; i++)
    {
        std::cout << d_train[i].first << ' ';
        for (auto& x : d_train[i].second)
            std::cout << x << ", ";
        std::cout << std::endl;
    }

    wine.train(d_train);

    std::cout << std::endl;
    wine.print();
    std::cout << std::endl;

    size_t ok = 0;
    //for (const auto& x : d_train) {
    for (const auto& x : d_test) {
        const auto p = wine.predict(x.second);
        if (x.first == (bool)p) ok++;
    }
    //std::cout << "OK is " << ok << " (" << ok / (double)d_train.size() * 100.<< "%)"
    std::cout << "OK is " << ok << " (" << ok / (double)d_test.size() * 100.<< "%)"
              << std::endl;
}

enum {SUN, OVERCAST, RAIN};
enum {HOT, COOL, MILD};
enum {HIGH, NORMAL};
enum {WEAK, STRONG};

void tennis_test()
{
    using DT = Dt<4>;
    DT tennis;

    std::cout << "t1: " << tennis.entropy(29/64.0) << " == 0.994" << std::endl;
    std::cout << "t2: " << tennis.entropy(21/26.0) << " == 0.706" << std::endl;

#if 1
    DT::List data = {
        //       outlook    temperature  humidity  wind
        {false, {SUN,       HOT,         HIGH,     WEAK}}, // 1
        {false, {SUN,       HOT,         HIGH,     STRONG}},
        {true,  {OVERCAST,  HOT,         HIGH,     WEAK}},
        {true,  {RAIN,      MILD,        HIGH,     WEAK}},
        {true,  {RAIN,      COOL,        NORMAL,   WEAK}}, // 5
        {false, {RAIN,      COOL,        NORMAL,   STRONG}},
        {true,  {OVERCAST,  COOL,        NORMAL,   STRONG}},
        {false, {SUN,       MILD,        HIGH,     WEAK}},
        {true,  {SUN,       COOL,        NORMAL,   WEAK}},
        {true,  {RAIN,      MILD,        NORMAL,   WEAK}}, // 10
        {true,  {SUN,       MILD,        NORMAL,   STRONG}},
        {true,  {OVERCAST,  MILD,        HIGH,     STRONG}},
        {true,  {OVERCAST,  HOT,         NORMAL,   WEAK}},
        {false, {RAIN,      MILD,        HIGH,     STRONG}}
    };
    // должно быть
    // feature 0, gain: 0.2467       // outlook
    // feature 1, gain: 0.0292       // temperature
    // feature 2, gain: 0.1518       // humidity
    // feature 3, gain: 0.0481       // wind
#else
    DT::List data = {
        //       outlook    temperature  humidity  wind
        {false, {SUN,       HOT,         HIGH,     WEAK}},
        {false, {SUN,       HOT,         HIGH,     STRONG}},
        {true,  {OVERCAST,  HOT,         HIGH,     WEAK}},
        {true,  {RAIN,      MILD,        HIGH,     WEAK}},
    };
#endif

    tennis.train(data);
    std::cout << std::endl;
    tennis.print();
    std::cout << std::endl;

    size_t ok = 0;
    size_t i = 0;
    for (const auto& x : data) {
        const auto p = tennis.predict(x.second);
        if (x.first == (bool)p) ok++;
        else std::cout << "error for row " << i << std::endl;
        i++;
    }
    std::cout << "OK is " << ok << " (" << ok / (double)data.size() * 100.<< "%)"
              << std::endl;
}

int main(void)
{
#if 0
    tennis_test();
#else
    wine_test();
#endif
    return 0;
}
