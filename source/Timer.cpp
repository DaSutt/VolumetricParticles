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

#include "Timer.h"

Timer::Timer()
  : secondsPerCount_(0.0),
  deltaTime_(-1.0),
  baseTime_(0),
  pauseTime_(0),
  stopTime_(0),
  prevTime_(0),
  currTime_(0),
  stopped_(false)
{
  __int64 countsPerSec;
  QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&countsPerSec));
  secondsPerCount_ = 1.0 / static_cast<double>(countsPerSec);
}

Timer::~Timer()
{

}

float Timer::GetTotalTime()const
{
  if (stopped_)
  {
    return static_cast<float>(((stopTime_ - pauseTime_) - baseTime_)*secondsPerCount_);
  }
  else
  {
    return static_cast<float>(((currTime_ - pauseTime_) - baseTime_)*secondsPerCount_);
  }
}

void Timer::Reset()
{
  __int64 currTime;
  QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));

  baseTime_ = currTime;
  prevTime_ = currTime;
  stopTime_ = 0;
  stopped_ = false;
}

void Timer::Start()
{
  __int64 startTime;
  QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&startTime));

  if (stopped_)
  {
    pauseTime_ += (startTime - stopTime_);
    prevTime_ = startTime;
    stopTime_ = 0;
    stopped_ = false;
  }
}

void Timer::Stop()
{
  if (!stopped_)
  {
    __int64 currTime;
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));

    stopTime_ = currTime;
    stopped_ = true;
  }
}

void Timer::Tick()
{
  if (stopped_)
  {
    deltaTime_ = 0.0;
    return;
  }

  __int64 currTime;
  QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));
  currTime_ = currTime;

  deltaTime_ = (currTime_ - prevTime_) * secondsPerCount_;

  prevTime_ = currTime_;

  if (deltaTime_ < 0.0)
  {
    deltaTime_ = 0.0;
  }

  CalculateFrameStats();
}

void Timer::CalculateFrameStats()
{
  static int frameCount;
  static float timeElapsed = 0.0f;

  frameCount++;

  if ((GetTotalTime() - timeElapsed) >= 0.0f)
  {
    fps_ = static_cast<float>(frameCount);
    mspf_ = 1000.0f / fps_;

    frameCount = 0;
    timeElapsed += 1.0f;
  }
}