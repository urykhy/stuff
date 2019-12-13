//
// call test with args from variants
//

#include <stdio.h>
#include <variant>
#include <string>
#include <array>
#include <iostream>

template<typename... Ts> void show() { puts(__PRETTY_FUNCTION__); }

template<typename... Ts>
void test(Ts &&...ts) {
  puts(__PRETTY_FUNCTION__);
  { ((std::cout << ts << '\n'),...); };
}

template<size_t N, typename... Vs>
static constexpr std::array<size_t, sizeof...(Vs)>
make_indices_array() {
  constexpr size_t len = sizeof...(Vs);
  constexpr size_t sizes[] = {
      std::variant_size_v<std::remove_reference_t<Vs>>...};
  std::array<size_t, len> indices{};
  size_t n = N;
  for (size_t i = 0; i < len; i++) {
    indices[i] = n % sizes[i];
    n = n / sizes[i];
  }
  return indices;
}

template<typename T>
struct indices_helper {};

template<size_t... Is>
struct indices_helper<std::integer_sequence<size_t, Is...>> {
  template<size_t N, typename... Vs>
  static constexpr auto make_indices() {
    constexpr auto arr = make_indices_array<N, Vs...>();
    return std::integer_sequence<size_t, arr[Is]...>();
  }
};

// return std::integer_sequence<size_t, Ind[0], Ind[1], ...>
// Ind-s are generated as follows (in a C++-like pseudocode):
// size_t n = N
// for (size_t i = 0; < sizeof...(Vs); i++) {
//    num_var = variant_size_v<Vs[i]>
//    Ind[i] = n % num_var
//    n = n / num_var
// }
template<size_t N, typename... Vs>
auto constexpr make_indices() {
  using ind_seq_t = std::make_index_sequence<sizeof...(Vs)>;
  return indices_helper<ind_seq_t>::template make_indices<N, Vs...>();
}

template<typename T>
struct lambda_helper {};

template<size_t... Is>
struct lambda_helper<std::integer_sequence<size_t, Is...>> {
  template<typename... Vs>
  static constexpr auto
  make() {
    return [](Vs... vs) {
      test(std::get<Is>(vs)...);
    };
  }
};

template<size_t N, typename... Vs>
constexpr auto make_lambda() {
  using indices_t = decltype(make_indices<N, Vs...>());
  return lambda_helper<indices_t>::template make<Vs...>();
}

template<typename... Vs>
constexpr size_t
count_num_combinations() {
  return (std::variant_size_v<std::remove_reference_t<Vs>> * ... * 1);
}

template<typename T>
struct make_funcs {};

template<size_t... Is>
struct make_funcs<std::integer_sequence<size_t, Is...>> {
  template<typename... Vs>
  static constexpr auto
  make() {
    std::array<void(*)(Vs...), count_num_combinations<Vs...>()> funcs = {
      make_lambda<Is, Vs...>()...
    };
    return funcs;
  }
};

template<typename... Vs>
void dispatch(const Vs&... vs) {
  constexpr size_t num_vs = sizeof...(Vs);
  constexpr size_t num_combs =
      (std::variant_size_v<std::remove_reference_t<Vs>> * ...);
  using comb_seq_t = std::make_index_sequence<num_combs>;
  constexpr auto funcs = make_funcs<comb_seq_t>::template make<const Vs&...>();
  constexpr std::array<size_t, num_vs> sizes = {
      std::variant_size_v<std::remove_reference_t<Vs>>...};
  std::array<size_t, num_vs> dyn_indices = {
    vs.index()...
  };
  size_t n = 0;
  for (int i = num_vs-1; i >= 0; --i)
    n = n * sizes[i] + dyn_indices[i];
  funcs[n](vs...);
}

int main() {
  std::variant<int, float> v1 = 0;
  std::variant<float, std::string> v2 = "aaa";
  dispatch(v1, v2);
  return 0;
}
