# Installation Guide<a name="EN-US_TOPIC_0000002553346771"></a>

<!-- md-trans-meta sourceCommit=cef0cf22901fa9ada7db6826684488ff749a38f9 translatedAt=2026-08-31T01:41:59.718Z pushedAt=2026-09-04T09:24:07.724Z -->

This document describes how to install Megascan via source code compilation and RPM packages. Source code compilation is suitable for development, debugging, and custom builds, whereas the RPM method is intended for deployment scenarios where an official software package has been obtained.

To use Megascan smoothly and securely, ensure that your environment is one of the verified environments. If you encounter any problem when using Megascan, check first whether your environment can be switched to a verified environment.

**Table 1** Verified environments<a id="verified-environments"></a>

|OS|CPU|GCC Version|
|--|--|--|
|openEuler 22.03 LTS SP4|New Kunpeng 920 processor model|10.3.1|
|openEuler 24.03 LTS SP1|New Kunpeng 920 processor model|12.3.1|

Compiling the Megascan dynamic library does not depend on third-party algorithm libraries such as Boost and PCRE.

|Software|Version|Purpose|
|--|--|--|
|GCC|10.3 or later|Mandatory, used for compiling the Megascan dynamic library|
|CMake|3.22 or later|Mandatory, used for generating the build project|
|GNU Make|Matching the OS|Mandatory, used for executing the default build|
|glibc and POSIX Threads|Matching the OS|Mandatory, used for providing the basic runtime library and thread interfaces|
|Python 3|3.8 or later|Optional, used for building test programs|
|CUnit|1.0 or later|Optional, used for running unit tests|
|rpm-build and tar|Matching the OS|Optional, used for building RPM packages|

Install the basic compilation tools on openEuler.

```bash
sudo dnf install -y gcc gcc-c++ cmake make
```

After the installation, check the versions.

```bash
gcc --version
cmake --version
make --version
```

## Source Code Compilation

The following commands are all executed in the root directory of the Megascan source code.

1. Go to the source code directory.

    ```bash
    cd Megascan
    ```

2. Generate the `Release` build directory.

    ```bash
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    ```

3. Compile the dynamic library.

    ```bash
    cmake --build build -j"$(nproc)"
    ```

4. Check the compilation outputs.

    ```bash
    test -f output/lib/libms.so && echo "libms.so: OK"
    test -f output/include/ms.h && echo "ms.h: OK"
    ```

    An example of command output is as follows:

    ```text
    libms.so: OK
    ms.h: OK
    ```

The main outputs after compilation are as follows:

|Path|Description|
|--|--|
|`output/lib/libms.so`|Megascan dynamic library|
|`output/include/*.h`|Public header files|
|`build/`|CMake intermediate files|

When you need to retain debug symbols and stack frame information, you can create a separate build directory.

```bash
cmake -S . -B build-profile \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DMS_ENABLE_PROFILING=ON
cmake --build build-profile -j"$(nproc)"
```

For rebuilding, delete the corresponding build directory and run the CMake commands again.

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

## Compilation and Testing

The test program depends on Python 3 and CUnit. You can install them first on openEuler.

```bash
sudo dnf install -y python3 CUnit-devel
```

If the `CUnit-devel` software package is not available in your environment, place the CUnit header files and libraries into the test project by following instructions in [KTest Test Framework](../../test/README.md).

After compiling the Megascan dynamic library, run the following commands in the root directory of the source code:

```bash
cd test
python3 build.py debug
```

Run the unit test.

```bash
./build/bin/testdemo_ut
```

View the demo help.

```bash
./build/bin/megascan_demo --help
```

The current directory is `Megascan/test`, so the two programs are actually generated at `Megascan/test/build/bin/testdemo_ut` and `Megascan/test/build/bin/megascan_demo`, respectively. For details about the KTest description, test pass conditions, and the coverage measurement method, see [KTest Test Framework](../../test/README.md). For the complete usage of the demo, see [Demo Usage Guide](../../test/demo/README.md).

## RPM Package Installation

If you have obtained the official RPM package, you can install it directly.

```bash
sudo rpm -ivh boostida-kdadk-ms-release-xxxx.aarch64.rpm
```

`xxxx` indicates the software version. After installation, the default file locations are as follows:

|Path|Description|
|--|--|
|`/usr/local/lib/libms.so`|Megascan dynamic library|
|`/usr/local/include/*.h`|Public header files|

Check the installation result:

```bash
rpm -q boostkit-kdadk-ms
rpm -ql boostkit-kdadk-ms
ldconfig -p | grep libms
```

The following is an example of the output upon successful installation. The version number and header file list are subject to the actual result.

```text
boostkit-kdadk-ms-1.1.0-oe2203.aarch64
/usr/local/lib/libms.so
/usr/local/include/ms.h
...
libms.so (libc6,AArch64) => /usr/local/lib/libms.so
```

To upgrade an installed package, run the following command:

```bash
sudo rpm -Uvh boostida-kdadk-ms-release-xxxx.aarch64.rpm
```

The following is an example of the output upon successful upgrade. The package name and progress are subject to the actual result.

```text
Verifying...                          ################################# [100%]
Preparing...                          ################################# [100%]
Updating / installing...
   1:boostkit-kdadk-ms-xxxx           ################################# [ 50%]
Cleaning up / removing...
   2:boostkit-kdadk-ms-xxxx           ################################# [100%]
```

## RPM Package Build

`build.sh` is used to build the RPM package in the AArch64 environment. Before executing it, install the packaging tools.

```bash
sudo dnf install -y rpm-build tar
```

After successful installation, the end of the `dnf` command output usually shows:

```text
Complete!
```

Run the following commands in the root directory of the source code:

```bash
chmod +x build.sh
./build.sh
```

The generated software package is located in the `output` directory. You can run the following command to confirm it:

```bash
ls -1 output/*.rpm
```

An example of command output is as follows:

```text
output/boostida-kdadk-ms-release-1.1.0.oe2203.aarch64.rpm
```

This script rebuilds the source code and organizes the packaging directory. It is not recommended for routine incremental compilation when only `libms.so` is needed.

## Development Environment Verification

After completing source code compilation or RPM package installation, you can compile the example program by following instructions in [Quick Start](./quick_start.md). If the example can successfully link `libms.so` and output the expected rule ID, the Megascan development environment is available.