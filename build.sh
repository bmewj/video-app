if [ ! -d "build" ]; then
    mkdir build
fi

cd build
cmake \
    -DCMAKE_BUILD_TYPE=DEBUG \
    -DCMAKE_CXX_COMPILER="clang++" \
    ..
make
cd ..
cd bin
./video-app
