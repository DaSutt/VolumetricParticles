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

#include "InputProps.h"

#include <glm\vec2.hpp>
#include <unordered_map>

class Mouse
{
public:
  Mouse(HWND clientHandle);
  ~Mouse();

  void ProcessInput();

  bool KeyDown(const VirtualKey vKey) const;
  bool KeyUp(const VirtualKey vKey);
  bool KeyUp(const VirtualKey vKey, glm::vec2& pos);
  bool KeyPressed(const VirtualKey vKey);
  bool KeyPressed(const VirtualKey vKey, glm::vec2& pos);

  void Reset();
  void OnMouseMove(int x, int y);
  void OnMouseDown(int x, int y);
  void OnMouseUp();

  void SetWindow(HWND newClientHandle);

  // returns the cursor position in client space
  // 0,0
  //     ...
  //         windowWidth, windowHeight
  glm::vec2 GetPointerPosition();

  const glm::vec2& GetDeltaAxis() const;

private:
  // Convert from screen to client space
  const void GetClientPosition(POINT* pos) const;

  std::unordered_map<VirtualKey, bool> state_ =
  {
    { VirtualKey::MS_LB, false },
    { VirtualKey::MS_RB, false },
    { VirtualKey::MS_MB, false }
  };

  bool isDragging_;
  HWND boundClientHandle_;
  POINT lastPos_;
  glm::vec2 mouseDelta_;
};