call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set CUDA_PATH=D:\Qtproject\medicalpro\tools\cuda-12.4.1-portable
set CUDAToolkit_ROOT=D:\Qtproject\medicalpro\tools\cuda-12.4.1-portable
set CUDACXX=D:\Qtproject\medicalpro\tools\cuda-12.4.1-portable\bin\nvcc.exe
set PATH=D:\Qtproject\medicalpro\tools\cuda-12.4.1-portable\bin;%PATH%
cmake -G Ninja -DCMAKE_MAKE_PROGRAM="D:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" -S "D:\Qtproject\medicalpro" -B "D:\Qtproject\medicalpro\build_ninja_cuda124_tests" -DBUILD_TESTING=ON -DCUDAToolkit_ROOT="D:\Qtproject\medicalpro\tools\cuda-12.4.1-portable" -DCMAKE_CUDA_COMPILER="D:\Qtproject\medicalpro\tools\cuda-12.4.1-portable\bin\nvcc.exe" -DCMAKE_CUDA_HOST_COMPILER="D:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe" -DCMAKE_CXX_COMPILER="D:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe"
if errorlevel 1 exit /b 1
cmake --build "D:\Qtproject\medicalpro\build_ninja_cuda124_tests" --target registration_core_meshgpu_compatibility_test -v
