pushd ..
rmdir /s build
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DPICO_BOARD=pico_w
mingw32-make
picotool load main.uf2
picotool reboot
popd
pause