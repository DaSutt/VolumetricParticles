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

#include "Mouse.h"

Mouse::Mouse(HWND clientHandle)
  : boundClientHandle_(clientHandle), mouseDelta_{ glm::vec2(0.0f, 0.0f) }
{
}

Mouse::~Mouse()
{
}

void Mouse::ProcessInput()
{
  for (auto& k : state_)
  {
    if (GetKeyState(k.first) & 0x8000)
    {
      k.second = true;
    }
    else
    {
      k.second = false;
    }
  }
}

bool Mouse::KeyDown(const VirtualKey vKey) const
{
  if (GetKeyState(vKey) & 0x8000)
  {
    return true;
  }
  return false;
}

bool Mouse::KeyUp(const VirtualKey vKey)
{
  if (((GetKeyState(vKey) & 0x8000) == 0) && (state_[vKey] == true))
  {
    return true;
  }

  return false;
}

bool Mouse::KeyUp(const VirtualKey vKey, glm::vec2& pos)
{
  if (((GetKeyState(vKey) & 0x8000) == 0) && (state_[vKey] == true))
  {
    pos = GetPointerPosition();

    return true;
  }

  return false;
}

bool Mouse::KeyPressed(const VirtualKey vKey)
{
  // if user pressed a button in this frame
  if ((GetKeyState(vKey) & 0x8000) && (state_[vKey] == false))
  {
    return true;
  }

  return false;
}

bool Mouse::KeyPressed(const VirtualKey vKey, glm::vec2& pos)
{
  // if user pressed a button in this frame
  if ((GetKeyState(vKey) & 0x8000) && (state_[vKey] == false))
  {
    pos = GetPointerPosition();

    return true;
  }

  return false;
}

void Mouse::Reset()
{
  mouseDelta_ = glm::vec2(0.0f, 0.0f);
}

void Mouse::OnMouseMove(int x, int y)
{
  mouseDelta_.x = x - static_cast<float>(lastPos_.x);
  mouseDelta_.y = y - static_cast<float>(lastPos_.y);

  lastPos_.x = x;
  lastPos_.y = y;
}

void Mouse::OnMouseDown(int x, int y)
{
  lastPos_.x = x;
  lastPos_.y = y;

  SetCapture(boundClientHandle_);
}

void Mouse::OnMouseUp()
{
  ReleaseCapture();
}

void Mouse::SetWindow(HWND newClientHandle)
{
  boundClientHandle_ = newClientHandle;
}

glm::vec2 Mouse::GetPointerPosition()
{
  POINT pos;
  this->GetClientPosition(&pos);

  return glm::vec2(static_cast<float>(pos.x), static_cast<float>(pos.y));
}

const glm::vec2& Mouse::GetDeltaAxis()const
{
  return mouseDelta_;
}

const void Mouse::GetClientPosition(POINT* pos) const
{
  if (!GetCursorPos(pos))
  {
    std::printf("[WARNING]: Failed to retrieve cursor position!\n");
  }

  if (!ScreenToClient(boundClientHandle_, pos))
  {
    std::printf("[WARNING]: Failed to convert from Screen to Client position!\n");
  }
}
