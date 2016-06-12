
#pragma once

// GSL
#include <gsl/gsl_multimin.h>

template<class T>
class LrGSL
{
    size_t iter = 0;
    int status = 0;
    gsl_vector *x = 0;
    gsl_multimin_fdfminimizer *s = 0;
    gsl_multimin_function_fdf my_func;

    using Vector = typename T::Vector;
    using List = typename T::List;

    // GSL API
    static double my_f (const gsl_vector *v, void *params)                  { return ((LrGSL*)params)->my_f_real(v);  }
    static void my_df (const gsl_vector *v, void *params, gsl_vector *df)   { ((LrGSL*)params)->my_df_real(v, df); }
    static void my_fdf (const gsl_vector *x, void *params, double *f, gsl_vector *df) { *f = my_f(x, params); my_df(x, params, df); }

    // IMPL
    double my_f_real(const gsl_vector *v)                { from_gsl(v); return lr->calc_error(training); }
    void my_df_real(const gsl_vector *v, gsl_vector *df) { from_gsl(v); to_gsl(lr->get_df(training), df); }

    // Hack to use Lr math, we copy x vector to Lr::w and back
    void from_gsl(const gsl_vector *x)                         { for (size_t i = 0; i < T::max_features; i++) w[i] = gsl_vector_get (x, i); }
    void to_gsl(const typename T::Vector& v, gsl_vector *dest) { for (size_t i = 0; i < T::max_features; i++) gsl_vector_set (dest, i, v[i]); }

    const typename T::List& training;
    typename T::Vector& w;
    T* lr;

public:

    LrGSL(T& t, const typename T::List& list)
    : training(list), w(t.w), lr(&t)
    {
        x = gsl_vector_alloc(T::max_features);
        for (size_t i = 0; i < T::max_features; i++) {
            gsl_vector_set (x, i, w[i]);
        }

        my_func.n = T::max_features;
        my_func.f = my_f;
        my_func.df = my_df;
        my_func.fdf = my_fdf;
        my_func.params = this;

        s = gsl_multimin_fdfminimizer_alloc(gsl_multimin_fdfminimizer_conjugate_fr, T::max_features);
        gsl_multimin_fdfminimizer_set (s, &my_func, x, 0.1, 1e-4);
    }

    ~LrGSL()
    {
        gsl_multimin_fdfminimizer_free(s);
        gsl_vector_free(x);
    }

    bool is_ok() const { return status == GSL_SUCCESS; }

    void train(const size_t max, double v = 1e-5)
    {
        do
        {
            iter++;
            status = gsl_multimin_fdfminimizer_iterate(s);
            if (status) {
                std::cerr << "gsl_multimin_fdfminimizer_iterate failed" << std::endl;
                break;
            }
            status = gsl_multimin_test_gradient(s->gradient, v);
            std::cout << "error at " << s->f << std::endl;
        }
        while (status == GSL_CONTINUE && iter < max);
        from_gsl(s->x);
    }

};

