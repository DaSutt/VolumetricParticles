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

#include <unordered_map>
#include "InputProps.h"

class Keyboard
{
public:
  Keyboard();
  ~Keyboard();

  void ProcessInput();

  bool KeyDown(const VirtualKey vKey);
  bool KeyUp(const VirtualKey vKey);
  bool KeyPressed(const VirtualKey vKey);

private:
	struct Status
	{
		bool pressed;
		bool previousPressed;
	};

  std::unordered_map<VirtualKey, Status> state_ =
  {
    { VirtualKey::KB_W,		{false, false} },
    { VirtualKey::KB_A,		{false, false} },
    { VirtualKey::KB_S,		{false, false} },
    { VirtualKey::KB_D,		{false, false} },
    { VirtualKey::KB_Q,		{false, false} },
    { VirtualKey::KB_E,		{false, false} },
		{ VirtualKey::KB_F1,	{false, false} }
  };
};