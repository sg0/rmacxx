cmake --build --preset default --target $1;
ctest --preset default -R $1 $2 $3 $4 $5;