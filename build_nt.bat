@echo off
set VSROOT=D:\Program Files\Microsoft Visual Studio\18\Community
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
cmake -S D:/dev/ktvlv/ktvlv -B D:/dev/ktvlv/ktvlv/build_ninja2 -G Ninja -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE=Release -DKTV_USE_LV_DRIVERS=OFF
ninja -C D:/dev/ktvlv/ktvlv/build_ninja2 -j 12

