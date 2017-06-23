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

#include "Keyboard.h"

Keyboard::Keyboard()
{
}

Keyboard::~Keyboard()
{
}

void Keyboard::ProcessInput()
{
  for (auto& k : state_)
  {
		k.second.previousPressed = k.second.pressed;
    if (GetKeyState(k.first) & 0x8000)
    {
      k.second.pressed = true;
    }
    else
    {
      k.second.pressed = false;
    }
  }
}

bool Keyboard::KeyDown(const VirtualKey vKey)
{
	return state_[vKey].pressed;
}

bool Keyboard::KeyUp(const VirtualKey vKey)
{
	return !state_[vKey].pressed && state_[vKey].previousPressed;
}

bool Keyboard::KeyPressed(const VirtualKey vKey)
{
	return state_[vKey].pressed && !state_[vKey].previousPressed;
}
