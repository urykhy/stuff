
#include <array>
#include <cmath>

#include <Lr.hpp>
#include <Parser.hpp>
#include <LrGSL.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

// here size is 13, since first element must be 1 for LR algorithm

void wine_test(bool gsl, const size_t steps = 100)
{
    using MODEL = LR<13>;
    MODEL wine;
    Parser<MODEL> loader;
    auto d = loader("wine.csv", "White");

    std::cout << "readed " << d.size() << " elements" << std::endl;

    size_t split_at = d.size() * 0.75;
    MODEL::List d_train(d.begin(), d.begin() + split_at);
    MODEL::List d_test(d.begin() + split_at, d.end());
    d.clear();

    std::cout << "train on " << d_train.size() << " elements" << std::endl;

    if (!gsl)
    {    // self implemented^Wbugged, SD
        for (size_t i = 0; i < steps; i++)
        {
            wine.train(d_train);
            std::cout << "last error: " << wine.calc_error(d_train) << std::endl;
        }
    } else {
        // GSL
        LrGSL<MODEL> gsl(wine, d_test);
        gsl.train(steps);
        if (gsl.is_ok()) {
            std::cout << "GSL solved" << std::endl;
        }
    }

    size_t ok = 0;
    for (const auto& x : d_test) {
        const auto p = wine.predict(x.second);
        if (x.first == false && p < 0.5) ok++;
        if (x.first == true  && p > 0.5) ok++;
    }
    std::cout << "OK is " << ok << "(" << ok / (double)d_test.size() * 100.<< "%)" << std::endl;
}

int main(int argc, char** argv)
{
    bool gsl = false;

    po::options_description desc("Program options");
    desc.add_options()
    ("help,h", "show usage information")
    ("gsl", po::bool_switch(&gsl), "use GSL/conjugate_fr solver");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }

    wine_test(gsl);

    return 0;
}

