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

#include "Application.h"

#include <iostream>
#include <assert.h>
#include <windowsx.h>
#include "fileIO\FileDialog.h"

#include "renderer\passes\GuiPass.h"
#include "utility\Status.h"

class ParticleSystemManager;

Application::Application()
{
  currWidth_ = 1600;
  currHeight_ = 900;
  window_ = std::make_unique<Window>(this, Application::WndProc, "Particle-Project", currWidth_, currHeight_);
  scene_ = std::make_shared<Scene>();
}

Application::~Application()
{}

bool Application::Init()
{
  scene_->OnResize(window_->GetWidth(), window_->GetHeight());

  inputHandler_.Init(window_->GetHWND());

  if (!sceneRenderer_.Init(scene_.get(), window_->GetHWND(), currWidth_, currHeight_))
  {
    return false;
  }
  sceneRenderer_.SetResources(scene_.get());

  timer_.Start();

  return true;
}

void Application::Release()
{
  sceneRenderer_.Release();

  int i;
  std::cin >> i;
}

void Application::Run()
{
  MSG msg = { 0 };
  while (WM_QUIT != msg.message)
  {
    if (PeekMessage(&msg, window_->GetHWND(), 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    else
    {
      timer_.Tick();
      inputHandler_.ProcessInput();
      Renderer::GuiPass::HandleInput(&inputHandler_);
      if (active_)
      {
        scene_->Update(&inputHandler_, timer_.GetDeltaTime());
      }

      sceneRenderer_.Update(scene_.get(), timer_.GetDeltaTime());
      sceneRenderer_.Render(scene_.get());

      HandleUIChanges();

      inputHandler_.Reset();
    }
  }
  timer_.Stop();
}

LRESULT CALLBACK Application::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  Application* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (app)
  {
    return app->AppWndProc(hwnd, uMsg, wParam, lParam);
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK Application::AppWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_CLOSE:
    window_->Close();
    return 0;
  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN:
    inputHandler_.OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    return 0;

  case WM_LBUTTONUP:
  case WM_RBUTTONUP:
  case WM_MBUTTONUP:
    inputHandler_.OnMouseUp();
    return 0;

  case WM_MOUSEMOVE:
    inputHandler_.OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    return 0;

  case WM_SIZE:
    changingWidth_ = LOWORD(lParam);
    changingHeight_ = HIWORD(lParam);
    if (wParam == SIZE_MAXIMIZED)
    {
      OnResize();
    }
    return 0;
  case WM_EXITSIZEMOVE:
    OnResize();
    return 0;
  case WM_ACTIVATE:
    active_ = (wParam != WA_INACTIVE);
    return 0;
  default:
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
}

void Application::HandleUIChanges()
{
  const auto menuState = Renderer::GuiPass::GetMenuState();
  if (menuState.loadScene)
  {
    const std::string path = FileIO::OpenFileDialog(L"scene");
    if (!path.empty())
    {
      scene_ = std::make_shared<Scene>();
      scene_->Load(path);
      scene_->OnResize(currWidth_, currHeight_);

      sceneRenderer_.OnLoadScene(scene_.get());

      //resourceManager_.LoadSceneTextures(renderer_->GetPhysicalDevice(), renderer_->GetDevice(),
      //  renderer_->GetQueue(), renderer_->GetCommandPool(), scene_.get());
      //resourceManager_.LoadSceneMeshes(renderer_->GetPhysicalDevice(), renderer_->GetDevice(),
      //  renderer_->GetQueue(), renderer_->GetCommandPool(), scene_.get());
  //  particleManager_.LoadScene(renderer_->GetQueue(), renderer_->GetCommandPool(),
  //    renderer_->GetDevice(), renderer_->GetPhysicalDevice(), scene_.get());
  //  bufferManager_.LoadScene(renderer_->GetPhysicalDevice(), renderer_->GetDevice(), scene_.get());*/
      timer_.Reset();
    }
  }
  if (menuState.reloadShaders)
  {
    sceneRenderer_.OnReloadShaders();
    timer_.Reset();
  }
  if (menuState.saveConfiguration)
  {
    Status::Save();
  }
}

void Application::OnResize()
{
  if (currWidth_ != changingWidth_ || currHeight_ != changingHeight_)
  {
    sceneRenderer_.OnResize(changingWidth_, changingHeight_);
    scene_->OnResize(changingWidth_, changingHeight_);
    currWidth_ = changingWidth_;
    currHeight_ = changingHeight_;
  }
}