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

class ParticleSystem;

namespace Renderer
{
  class BufferManager;

  //TODO allocate and bind buffer for device and staging
  //map buffer memory and copy alive particles
  //dispatch call to update grid
  class ParticleSystemRenderable
  {
  public:
    ParticleSystemRenderable();

    bool Init(ParticleSystem* particleSystem, BufferManager* bufferManager);
    void Update(BufferManager* bufferManager);
    void FinishUpdate(BufferManager* bufferManager);
    void AddToVolume();

    int GetBufferIndex() const { return bufferIndex_; }
    int GetActiveParticles() const;
  private:
    ParticleSystem* particleSystem_;
    int bufferIndex_ = -1;
  };
}