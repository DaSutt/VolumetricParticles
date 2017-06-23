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

class Timer
{
public:
  Timer();
  ~Timer();

  float GetTotalTime()const;
  const float GetDeltaTime() const { return static_cast<float>(deltaTime_); }
  const double GetDeltaTimeD() const { return deltaTime_; }

  void Reset();
  void Start();
  void Stop();
  void Tick();

  float GetFPS()const { return fps_; }
  float GetMSPF()const { return mspf_; }
private:
  void CalculateFrameStats();

  double secondsPerCount_;
  double deltaTime_;

  __int64 baseTime_;
  __int64 pauseTime_;
  __int64 stopTime_;
  __int64 prevTime_;
  __int64 currTime_;

  bool stopped_;

  float fps_;
  float mspf_;
};
