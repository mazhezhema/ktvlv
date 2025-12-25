@echo off
setlocal
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S D:/dev/ktvlv/ktvlv -B D:/dev/ktvlv/ktvlv/build_ninja -G Ninja -DCMAKE_SYSTEM_VERSION=10.0 -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE=Release
cmake --build D:/dev/ktvlv/ktvlv/build_ninja --config Release

