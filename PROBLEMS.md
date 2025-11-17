# PROBLEMS

- 2025-11-16: `C:\msys64\ucrt64\bin\g++.exe` fails to compile even a trivial program when invoked from this shell, so `cmake` configuration with the Ninja generator aborts before generating the build files. Reproduce with `& 'C:\msys64\ucrt64\bin\cmake.exe' -S . -B build -G Ninja -D CMAKE_MAKE_PROGRAM=C:/msys64/ucrt64/bin/ninja.exe -D CMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -D CMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe`.
- 2025-11-16: **Resolved** — once VS Code picked up the MSYS2 UCRT64 environment, `cmake -S . -B build -G Ninja` and subsequent builds/tests succeed without additional configuration.
