cd ThirdParty
git clone https://github.com/g-truc/glm
git clone https://github.com/ocornut/imgui.git
git clone https://github.com/nothings/stb.git
git clone https://github.com/google/shaderc shaderc

cd shaderc/third_party
git clone https://github.com/google/googletest.git
git clone https://github.com/google/glslang.git
git clone https://github.com/KhronosGroup/SPIRV-Tools.git spirv-tools
git clone https://github.com/KhronosGroup/SPIRV-Headers.git spirv-tools/external/spirv-headers
cd ..
md bin
cd bin
cmake .. -G "Visual Studio 15 2017 Win64"
cmake --build . --config Debug
cmake --build . --config Release

cd ..\..\..\
md bin
cd bin
cmake .. -G "Visual Studio 15 2017 Win64"
cmake --build . --config Debug
cmake --build . --config Release

cd ..
robocopy data\Noise bin\data\Noise /s /e