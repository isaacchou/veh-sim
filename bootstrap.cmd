@echo off
rem
rem This script sets up third-party libraries
rem It sits above the repo directory so can be shared by multiple projects
rem
pushd ..
if not exist third_party mkdir third_party

cd third_party

rem use includes and pre-built binaries
if exist glfw ( 
    echo ===
    echo === Skipping glfw
    echo ===
) else (
    echo ===
    echo === Setting up glfw
    echo ===
    curl -LO https://github.com/glfw/glfw/releases/download/3.3.8/glfw-3.3.8.bin.WIN64.zip
    tar xf glfw-3.3.8.bin.WIN64.zip
    rename glfw-3.3.8.bin.WIN64 glfw
    del glfw-3.3.8.bin.WIN64.zip
)

rem use deps and glad includes
if exist glfw-src ( 
    echo ===
    echo === Skipping glfw-src
    echo ===
) else (
    echo ===
    echo === Setting up glfw-src
    echo ===
    curl -LO https://github.com/glfw/glfw/releases/download/3.3.8/glfw-3.3.8.zip
    tar xf glfw-3.3.8.zip
    rename glfw-3.3.8 glfw-src
    del glfw-3.3.8.zip
)

if exist glm (
    echo ===
    echo === Skipping glm
    echo ===
) else (
    echo ===
    echo === Setting up glm
    echo ===
    curl -LO https://github.com/g-truc/glm/releases/download/0.9.9.8/glm-0.9.9.8.zip
    tar xf glm-0.9.9.8.zip
    del glm-0.9.9.8.zip
)

if exist stb (
    echo ===
    echo === Skipping stb
    echo ===
) else (
    echo ===
    echo === Setting up stb
    echo ===
    curl -Lo stb-master.zip https://github.com/nothings/stb/archive/refs/heads/master.zip
    tar xf stb-master.zip
    rename stb-master stb
    del stb-master.zip
)

rem the following third party components need vcpkg
if not exist vcpkg-2023.04.15 (
    echo ===
    echo === Installing vcpkg
    echo ===
    curl -Lo vcpkg.zip https://github.com/microsoft/vcpkg/archive/refs/tags/2023.04.15.zip
    tar xf vcpkg.zip
    del vcpkg.zip
    pushd vcpkg-2023.04.15
    call bootstrap-vcpkg.bat
    popd
)

if exist bullet3 (
    echo ===
    echo === Skipping bullet3
    echo ===
) else (
    echo ===
    echo === Setting up bullet3
    echo ===
    pushd vcpkg-2023.04.15
    vcpkg install bullet3:x64-windows-static
    xcopy packages\bullet3_x64-windows-static ..\bullet3 /EIQ
    popd
)

if exist rapidjson (
    echo ===
    echo === Skipping rapidjson
    echo ===
) else (
    echo ===
    echo === Setting up rapidjson
    echo ===
    pushd vcpkg-2023.04.15
    vcpkg install rapidjson:x64-windows
    xcopy packages\rapidjson_x64-windows\include ..\rapidjson /EIQ
    popd
)

if exist boost_1_82_0 (
    echo ===
    echo === Skipping boost
    echo ===
) else (
    echo ===
    echo === Setting up boost
    echo ===
    curl -LO https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.zip
    tar xf boost_1_82_0.zip
    del boost_1_82_0.zip
)

popd

echo ===
echo === Bootstrap complete
echo ===
