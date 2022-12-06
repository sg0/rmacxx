module load cmake/3.17.0
module load mpich/3.3.2
module load gcc/10.2.1
mkdir build
cd build
cmake ../
cmake --build ../
ctest .
cd ../ 
ctest .
