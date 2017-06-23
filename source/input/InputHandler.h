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

#include <Windows.h>
#include <memory>
#include "InputProps.h"

#include "Mouse.h"
#include "Keyboard.h"

class InputHandler
{
public:
  InputHandler();
  ~InputHandler();

  void Init(HWND clientHandle);

  void SetWindow(HWND clientHandle);

  void ProcessInput();
  void Reset();

  bool KeyDown(const VirtualKey vKey);
  bool KeyUp(const VirtualKey vKey);
  bool KeyPressed(const VirtualKey vKey);

  bool MouseDown(const VirtualKey vKey) const;
  bool MouseUp(const VirtualKey vKey);
  bool MouseUp(const VirtualKey vKey, glm::vec2& pos);
  bool MousePressed(const VirtualKey vKey);
  bool MousePressed(const VirtualKey vKey, glm::vec2& pos);
  void OnMouseMove(int x, int y);
  void OnMouseDown(int x, int y);
  void OnMouseUp();
  glm::vec2 MousePosition();
  const glm::vec2& MouseDeltaAxis() const;

private:
  std::unique_ptr<Mouse> mouse_;
  std::unique_ptr<Keyboard> keyboard_;
};