#include "rmacxx.hpp"
#include <list>
#include <array>
#include <cassert>

int main(int argc, char *argv[]) { 
    std::vector<int> v1(3);
    std::vector<int> v2(3);
    std::vector<int> v3(6);
    std::list<int> l1(3);
    std::array<int,3> a1;

    const bool v1v2_ok = rmacxx::same_size(v1, v2);
    assert(v1v2_ok);
    const bool v2v1_ok = rmacxx::same_size(v2, v1);
    assert(v2v1_ok);
    const bool v1v3_ok = rmacxx::same_size(v1, v3);
    assert(!v1v3_ok);
    const bool v1l1_ok = rmacxx::same_size(v1, l1);
    assert(v1l1_ok);
    const bool v1a1_ok = rmacxx::same_size(v1, a1);
    assert(v1a1_ok);
}