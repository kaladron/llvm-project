# Building an LLVM Libc Sysroot

This document describes the steps to build a sysroot using LLVM libc.

## 1. Create the Sysroot Directory

First, create a directory to house the sysroot. We will use `~/sysroot` for this guide.

```bash
mkdir -p ~/sysroot
```

## 2. Setup Kernel Headers

Next, install the Linux kernel headers into the sysroot. For this guide, we'll copy the headers from the host system's `/usr/include` directory. This includes `linux`, `asm-generic`, and the architecture-specific `asm` headers.

```bash
# Create the include directory
mkdir -p ~/sysroot/usr/include

# Copy the header directories
cp -R /usr/include/linux ~/sysroot/usr/include/
cp -R /usr/include/asm-generic ~/sysroot/usr/include/
# Use -L to dereference the asm symlink and copy the actual files
cp -R -L /usr/include/asm ~/sysroot/usr/include/
```

**Note:** For a more production-ready sysroot, you would typically download a specific kernel version and install the headers using `make headers_install` configured for the target architecture and installation path.

## 3. Configure the LLVM Libc Build

Now, configure the build for LLVM libc and compiler-rt. We will build from the `runtimes` subdirectory of the LLVM project.

Create a build directory and run CMake:

```bash
mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE:STRING=Debug \
  -DLLVM_ENABLE_RUNTIMES:STRING='libc;compiler-rt' \
  -DCLANG_DEFAULT_LINKER:STRING=lld \
  -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
  -DLIBC_CMAKE_VERBOSE_LOGGING:BOOL=TRUE \
  -DCMAKE_INSTALL_PREFIX:STRING=/usr/local/google/home/jeffbailey/sysroot \
  -DDEFAULT_SYSROOT:STRING=/usr/local/google/home/jeffbailey/sysroot \
  -DCMAKE_BUILD_WITH_INSTALL_RPATH:BOOL=ON \
  -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/clang-19 \
  -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/clang++-19 \
  -DCOMPILER_RT_BUILD_BUILTINS:BOOL=TRUE \
  -DCOMPILER_RT_BUILD_CRT:BOOL=TRUE \
  -DCOMPILER_RT_BUILD_GWP_ASAN:BOOL=FALSE \
  -DLLVM_LIBC_FULL_BUILD:BOOL=TRUE \
  -DLLVM_ENABLE_SPHINX:BOOL=FALSE \
  -DLIBC_INCLUDE_DOCS:BOOL=FALSE \
  -DLLVM_LIBC_INCLUDE_SCUDO:BOOL=TRUE \
  -DCOMPILER_RT_BUILD_SCUDO_STANDALONE_WITH_LLVM_LIBC:BOOL=TRUE \
  -DCOMPILER_RT_SCUDO_STANDALONE_BUILD_SHARED:BOOL=FALSE \
  -Wno-dev --no-warn-unused-cli \
  -S ../runtimes \
  -G Ninja
```

```
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=$HOME/sysroot/usr \
  -DCMAKE_C_COMPILER_TARGET=x86_64-linux-gnu \
  -DCMAKE_CXX_COMPILER_TARGET=x86_64-linux-gnu \
  -DCOMPILER_RT_BUILD_BUILTINS:BOOL=TRUE \
  -DCOMPILER_RT_BUILD_CRT:BOOL=TRUE \
  -DCOMPILER_RT_BUILD_SANITIZERS=ON \
  -DCOMPILER_RT_BUILD_STANDALONE_LIBATOMIC=ON \
  -DCOMPILER_RT_SCUDO_STANDALONE_BUILD_SHARED=ON \
  -DCOMPILER_RT_BUILD_GWP_ASAN:BOOL=FALSE \
  -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
  -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON \
  ../compiler-rt
```

**Notes on CMake options:**

*   `-DLLVM_ENABLE_RUNTIMES:STRING='libc;compiler-rt'`: We are only building the libc and compiler-rt runtimes.
*   `-DCMAKE_INSTALL_PREFIX:STRING=/usr/local/google/home/jeffbailey/sysroot`: This sets the installation directory to our sysroot.
*   `-DDEFAULT_SYSROOT:STRING=/usr/local/google/home/jeffbailey/sysroot`: This ensures the build system knows about our sysroot.
*   `-DCMAKE_C_COMPILER:FILEPATH=/usr/bin/clang-19` and `-DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/clang++-19`: Specify the compilers to use for building the runtimes.
*   `-DCOMPILER_RT_BUILD_BUILTINS:BOOL=ON` and `-DCOMPILER_RT_BUILD_CRT:BOOL=TRUE`: Ensure the necessary compiler-rt components are built.
*   `-S ../runtimes`: We are building from the `runtimes` source directory.

## 4. Build and Install LLVM Libc

With the build system configured, build and install LLVM libc using the `install-libc` target. This will install the headers, static libraries, and CRT startup files into the sysroot.

```bash
ninja -C build install-libc
```

## 5. Build and Install Compiler-RT CRT Files

Next, install the compiler-rt CRT files. These are necessary for the linker to find the startup code.

```bash
ninja -C build install-crt
```

This will install files like `clang_rt.crtbegin-x86_64.o` and `clang_rt.crtend-x86_64.o` into `~/sysroot/lib/linux/`.

## 6. Configure and Build Clang

Now, we need to build Clang itself, configured to use the sysroot we've been populating. Create a new build directory and run CMake. This time, we source from the main `llvm` directory.

```bash
mkdir -p build-clang
cd build-clang

cmake \
  -DCMAKE_BUILD_TYPE:STRING=Debug \
  -DLLVM_ENABLE_PROJECTS:STRING='clang;lld' \
  -DLLVM_ENABLE_RUNTIMES:STRING='' \
  -DCLANG_DEFAULT_LINKER:STRING=lld \
  -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
  -DCMAKE_INSTALL_PREFIX:STRING=/usr/local/google/home/jeffbailey/sysroot/usr \
  -DDEFAULT_SYSROOT:STRING=/usr/local/google/home/jeffbailey/sysroot \
  -DCMAKE_BUILD_WITH_INSTALL_RPATH:BOOL=ON \
  -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/clang-19 \
  -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/clang++-19 \
  -DLLVM_DEFAULT_TARGET_TRIPLE:STRING=x86_64-linux-llvm \
  -DCOMPILER_RT_DEFAULT_TARGET_TRIPLE:STRING=x86_64-linux-llvm \
  -Wno-dev --no-warn-unused-cli \
  -S ../llvm \
  -G Ninja
```

**Notes on CMake options:**

*   `-DLLVM_ENABLE_PROJECTS:STRING='clang;lld'`: We are building Clang and LLD.
*   `-DLLVM_ENABLE_RUNTIMES:STRING=''` : We don't need to build runtimes again.
*   `-DCMAKE_INSTALL_PREFIX:STRING=/usr/local/google/home/jeffbailey/sysroot/usr`: Install Clang into the sysroot under `usr`.
*   `-DDEFAULT_SYSROOT:STRING=/usr/local/google/home/jeffbailey/sysroot`: Point Clang to our new sysroot.
*   `-DLLVM_DEFAULT_TARGET_TRIPLE:STRING=x86_64-linux-llvm`: Set the default target to our LLVM libc triple.

Now, build and install Clang:

```bash
ninja -C build-clang install-clang install-lld
```

To ensure all necessary headers like `stdarg.h` are available, install the Clang resource headers:

```bash
ninja -C build-clang install-clang-resource-headers
```
