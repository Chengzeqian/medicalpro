call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set CUDA_PATH=D:\Qtproject\medicalpro\tools\cuda-12.4.1-portable
set CUDAToolkit_ROOT=D:\Qtproject\medicalpro\tools\cuda-12.4.1-portable
set CUDACXX=D:\Qtproject\medicalpro\tools\cuda-12.4.1-portable\bin\nvcc.exe
set PATH=D:\Qtproject\medicalpro\tools\cuda-12.4.1-portable\bin;%PATH%
cmake --build "D:\Qtproject\medicalpro\build_ninja_cuda124" --target registration_core_meshgpu_compatibility_test -v
