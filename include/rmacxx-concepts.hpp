// #define DEBUG

template< class T >
inline constexpr bool is_copy_constructible_v = is_copy_constructible<T>::value;

// template<typename T>
// concept SameSize =
//     requires(T a, T b) {
//         ()
// };

// template<typename T>
// concept CheckLeftShift =
//     requires(T a, T b){
// };