md ThirdParty
cd ThirdParty
git clone https://github.com/g-truc/glm
git clone https://github.com/ocornut/imgui.git
git clone https://github.com/nothings/stb.git
cd ..
md bin
cd bin
cmake .. -G "Visual Studio 15 2017 Win64"