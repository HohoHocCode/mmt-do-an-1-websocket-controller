rm -rf build

cmake -B build -G Ninja \
  -DCMAKE_MAKE_PROGRAM=/mingw64/bin/ninja \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-mingw-static

cmake --build build
