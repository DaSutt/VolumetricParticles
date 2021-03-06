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

#pragma once
#define VK_USE_PLATFORM_WIN32_KHR

#include <memory>
#include <vector>
#include <vulkan\vulkan.h>

#include "Window.h"
#include "input/InputHandler.h"
#include "Timer.h"
#include "scene\Scene.h"

#include "renderer\SceneRenderer.h"

class Application
{
public:
  Application();
  ~Application();

  bool Init();
  void Release();
  void Run();

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
private:
  LRESULT CALLBACK AppWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  void HandleUIChanges();
  void OnResize();

  std::unique_ptr<Window> window_;
  InputHandler inputHandler_;
  Timer timer_;
  std::shared_ptr<Scene> scene_;

  Renderer::SceneRenderer sceneRenderer_;

  int currWidth_;
  int currHeight_;

  int changingWidth_;
  int changingHeight_;

  bool active_ = true;
};