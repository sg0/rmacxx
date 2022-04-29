// #define DEBUG

template<typename T>
concept CopyConstructable = std::is_copy_constructabl<T>;

// template<typename T>
// concept SameSize =
//     requires(T a, T b) {
//         ()
// };

// template<typename T>
// concept CheckLeftShift =
//     requires(T a, T b){
// };