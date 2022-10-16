#include "rmacxx.hpp"
#include <list>
#include <cassert>

int main(int argc, char *argv[]) { 
    std::string x = "hello";
    int const y = -3;
    int const z = 0;
    unsigned int const a = 3;

    const bool int0_ok = rmacxx::is_pos(y);
    assert(!int0_ok);
    const bool int1_ok = rmacxx::is_pos(z);
    assert(!int1_ok);
    const bool int2_ok = rmacxx::is_pos(a);
    assert(int2_ok);
}