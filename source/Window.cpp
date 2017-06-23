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

#include "Window.h"

Window::Window(Application* app, WNDPROC windowProc, const std::string& title, int width, int height) :
  title_{title}, width_{width}, height_{height}
{
  WNDCLASSEXA wcex = {};
  wcex.cbSize = sizeof(WNDCLASSEXA);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = windowProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = GetModuleHandle(NULL);
  wcex.hIcon = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_APPLICATION);
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcex.lpszMenuName = nullptr;
  wcex.lpszClassName = "Particle-ProjectWindowClass";
  wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_APPLICATION);
  const auto classAtom = RegisterClassExA(&wcex);
  if (!classAtom)
  {
    printf("RegisterClassA failed %d", GetLastError());
    return;
  }

  RECT rc = { 0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_) };
  // init the drag area of the input
  AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	const int windowWidth = rc.right - rc.left;
	const int windowHeight = rc.bottom - rc.top;

  hwnd_ = CreateWindowA(wcex.lpszClassName, title_.c_str(), WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight, NULL, NULL, wcex.hInstance, NULL);
  if (!hwnd_)
  {
    printf("CreateWindowA failed %d", GetLastError());
    return;
  }

  SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));

  ShowWindow(hwnd_, SW_SHOWDEFAULT);
  UpdateWindow(hwnd_);
}

HWND Window::GetHWND() const
{
  return hwnd_;
}

int Window::GetWidth() const
{
  return width_;
}

int Window::GetHeight() const
{
  return height_;
}

void Window::Close()
{
  DestroyWindow(hwnd_);
  hwnd_ = NULL;
  UnregisterClassA(className_.c_str(), GetModuleHandle(NULL));
  PostQuitMessage(0);
  closed_ = true;
}