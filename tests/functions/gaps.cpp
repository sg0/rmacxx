#include "rmacxx.hpp"
#include <list>

#include <cassert>
#include <unistd.h>

int main(int argc, char *argv[]) {

    std::vector<std::vector<int>> los, his;

    //std::vector<int> print_test{1, 2, 4, 2, 2, 1, 23, 43, 4};
    //rmacxx::print_vector(print_test);
    
    std::vector<int> los1, los2, los3, los4, his1, his2, his3, his4;

    los1.push_back(0); los1.push_back(0); los1.push_back(0);
    his1.push_back(5); his1.push_back(5); his1.push_back(9);

    los2.push_back(6); los2.push_back(0); los2.push_back(0);
    his2.push_back(9); his2.push_back(5); his2.push_back(9);
    
    los3.push_back(0); los3.push_back(6); los3.push_back(0);
    his3.push_back(2); his3.push_back(9); his3.push_back(9);

    los4.push_back(3); los4.push_back(6); los4.push_back(0);
    his4.push_back(9); his4.push_back(9); his4.push_back(9);
    
    los.push_back(los1); los.push_back(los2); los.push_back(los3); los.push_back(los4);
    his.push_back(his1); his.push_back(his2); his.push_back(his3); his.push_back(his4); 
    
    std::vector<int> winsize{10, 10, 10};

    rmacxx::print_vector(los[0]);


    std::cout<<"RESULT: "<<rmacxx::check_gaps(winsize, los, his)<<std::endl;

    return 0;
}

