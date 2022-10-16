mpic++ $1.cpp -O3 -Wall -std=c++17 -pthread -I./include -D_GNU_SOURCE -DTEST_OVERHEAD -o $1 &&
mpiexec -np $2 $1 &&
rm $1;