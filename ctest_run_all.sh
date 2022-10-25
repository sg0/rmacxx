cmake --preset default;
cmake --build --preset default;
ctest --preset default $1 $2 $3 $4 $5;