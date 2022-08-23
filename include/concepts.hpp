#include "rmacxx.hpp"
#include <concepts>
#include <ranges>

//proof of concept -> concept must be defined as a constexpr bool, and all concepts are that
template <typename T>
concept always_satisfied = true;

// Check if number is positive (Not Working Correctly)
template <typename T>
concept is_positive = requires(T v) {
    std::is_integral_v<T>;
    {v > 0};
};

// Check if data type is contiguous (Working)
template <typename T>
concept is_contig = std::contiguous_iterator<T>;

// ensure we pass in valid arguments into the << operator
template<class T, class W, class S>
concept CheckCoords = requires( W win, S sub /*, std::vector<int> nlo, std::vector<int> nhi, std::vector<int> sizes, int i */) {
   { win } -> std::same_as<rmacxx::Window<T, GLOBAL_VIEW>>;
   { sub } -> std::same_as<rmacxx::RMACXX_Subarray_t<T, GLOBAL_VIEW>>;


    /*
    //if we're given size
    //std::vector<int> nlo; std::vector<int> nhi;
    nlo.insert(nlo.end(), win.store_lo.begin(), win.store_lo.end());
    nhi.insert(nhi.end(), win.store_hi.begin(), win.store_hi.end());

    //take all the hi and lo values and convert them into sizes
    //std::vector<int> sizes(nhi.size());
    // for (int i = 0; i < nlo.size(); i++) {
    //     sizes[i] = nhi[i] - nlo[i] + 1;
    // }

    while (i < nlo.size()) {
        sizes[i] = nhi[i] - nlo[i] + 1;
        i++;
    }

    if (origin.sizes_ != sizes) {
        //failed check
        std::cout << "Coordinates not equal" << std::endl;
        abort();
    }
    */

};