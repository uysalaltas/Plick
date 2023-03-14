pushd ..
rmdir /s build
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DPICO_BOARD=pico_w
mingw32-make
popd
pause