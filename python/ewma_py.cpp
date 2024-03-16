#include <boost/python.hpp>

#include <unsorted/Ewma.hpp>

using namespace boost::python;

BOOST_PYTHON_MODULE(libewma_py)
{
    using Util::Ewma;

    class_<Ewma>("Ewma", init<double, double>())
        .def("add", &Ewma::add)
        .def("estimate", &Ewma::estimate)
        .def("reset", &Ewma::reset);
}