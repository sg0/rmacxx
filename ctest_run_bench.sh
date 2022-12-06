cmake --preset default;
cmake --build --preset default;
ctest --preset bench $1 $2 $3 $4 $5;