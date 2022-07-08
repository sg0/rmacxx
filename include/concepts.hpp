#include "rmacxx.hpp"
#include <concepts>
#include <ranges>

// data type is contiguous
template<class T> 
inline constexpr bool is_contig = static_assert(std::contiguous_iterator<T>) || static_assert(std::ranges::contiguous_range<T>);

//template <typename T>
//concept is_contig = std::contiguous_iterator<T>;

// only positive coordinates

// user is using global and local view correctly




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