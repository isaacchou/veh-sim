@echo off

if not exist gl-matrix-min.js (
    echo setting up gl-matrix-min.js
    if not exist ..\..\third_party\gl-matrix-3.4.1 (
        echo Please run bootstrap.cmd to setup gl-matrix javascript library
        goto done 
    )
    copy ..\..\third_party\gl-matrix-3.4.1\dist\gl-matrix-min.js .
)

pushd ..\x64\Release
start veh-sim.exe server=9001 ..\..\scene_desc.json
popd

start python -m http.server 9000

:done
