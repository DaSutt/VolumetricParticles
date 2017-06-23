/*
MIT License

Copyright(c) 2017 Daniel Suttor

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "OutputManager.h"

#include <json11\json11.hpp>
#include <fstream>

#include "..\Camera.h"

using namespace json11;

void SaveConfiguration(Camera* camera)
{
  std::ofstream file;
  file.open("configuration.txt");
  if (file.is_open())
  {
    const auto& pos = camera->GetPosition();
		const auto& lookAt = camera->GetForward();
		const float fov = camera->GetFOVHorizontal();

    Json cameraConfig = Json::object{
      {"Position", Json::array{pos.x, pos.y, pos.z}},
			{"Look At", Json::array{lookAt.x, lookAt.y, lookAt.z}},
			{"FOV", Json::array{fov}}
    };

    file << cameraConfig.dump();

    file.close();
  }
}