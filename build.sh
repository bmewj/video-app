if [ ! -d "build" ]; then
    mkdir build
fi

cd build
cmake \
    -DCMAKE_CXX_COMPILER="clang++" \
    -DCMAKE_BUILD_TYPE=DEBUG \
    ..
make
cd ..
cd bin
./video-app