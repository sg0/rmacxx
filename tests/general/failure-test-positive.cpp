#include "rmacxx.hpp"

int main(int argc, char *argv[]) { 
    int x = 3;
    int y = -3;
    int z = 0;

    rmacxx::is_positive auto foo = x;
    rmacxx::is_positive auto bar = y;
    rmacxx::is_positive auto nil = z;

}