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

#include "InputHandler.h"

InputHandler::InputHandler()
{
}

InputHandler::~InputHandler()
{
}

void InputHandler::Init(HWND clientHandle)
{
  mouse_ = std::make_unique<Mouse>(clientHandle);
  keyboard_ = std::make_unique<Keyboard>();
}

void InputHandler::SetWindow(HWND clientHandle)
{
  mouse_->SetWindow(clientHandle);
}

void InputHandler::ProcessInput()
{
  mouse_->ProcessInput();
  keyboard_->ProcessInput();
}

void InputHandler::Reset()
{
  mouse_->Reset();
}

bool InputHandler::KeyDown(const VirtualKey vKey)
{
  return keyboard_->KeyDown(vKey);
}

bool InputHandler::KeyUp(const VirtualKey vKey)
{
  return keyboard_->KeyUp(vKey);
}

bool InputHandler::KeyPressed(const VirtualKey vKey)
{
  return keyboard_->KeyPressed(vKey);
}

bool InputHandler::MouseDown(const VirtualKey vKey) const
{
  return mouse_->KeyDown(vKey);
}

bool InputHandler::MouseUp(const VirtualKey vKey)
{
  return mouse_->KeyUp(vKey);
}

bool InputHandler::MouseUp(const VirtualKey vKey, glm::vec2& pos)
{
  return mouse_->KeyUp(vKey, pos);
}

bool InputHandler::MousePressed(const VirtualKey vKey)
{
  return mouse_->KeyPressed(vKey);
}

bool InputHandler::MousePressed(const VirtualKey vKey, glm::vec2& pos)
{
  return mouse_->KeyPressed(vKey, pos);
}

void InputHandler::OnMouseMove(int x, int y)
{
  return mouse_->OnMouseMove(x, y);
}

void InputHandler::OnMouseDown(int x, int y)
{
  return mouse_->OnMouseDown(x, y);
}

void InputHandler::OnMouseUp()
{
  return mouse_->OnMouseUp();
}

glm::vec2 InputHandler::MousePosition()
{
  return mouse_->GetPointerPosition();
}

const glm::vec2& InputHandler::MouseDeltaAxis() const
{
  return mouse_->GetDeltaAxis();
}