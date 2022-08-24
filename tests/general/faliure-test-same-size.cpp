#include "rmacxx.hpp"

int main(int argc, char *argv[]) {

    std::vector<int> lsizes = {1,2,3}, sizes = {1,2,3};

    //rmacxx::is_contig auto foo = buff;
    rmacxx::same_size auto foo = (lsizes, sizes);
}