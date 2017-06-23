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

#include "ParticleSystemRenderable.h"

#include "resources\BufferManager.h"
#include "..\scene\ParticleSystem.h"

namespace Renderer
{
  ParticleSystemRenderable::ParticleSystemRenderable()
  {}

  bool ParticleSystemRenderable::Init(ParticleSystem* sceneSystem, BufferManager* bufferManager)
  {
    particleSystem_ = sceneSystem;

    BufferManager::BufferInfo bufferInfo;
    bufferInfo.typeBits = BufferManager::BUFFER_SCENE_BIT | BufferManager::BUFFER_STAGING_BIT;
    bufferInfo.size = particleSystem_->GetParticleCount() * sizeof(glm::vec3);
    bufferInfo.pool = BufferManager::MEMORY_SCENE;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferIndex_ = bufferManager->RequestBuffer(bufferInfo);

    return true;
  }

  void ParticleSystemRenderable::Update(BufferManager* bufferManager)
  {
    auto dataPtr = bufferManager->Map(bufferIndex_, BufferManager::BUFFER_STAGING_BIT);
    const auto copySize = particleSystem_->GetAliveCount() * sizeof(glm::vec3);
    memcpy(dataPtr, particleSystem_->GetPositions().data(), copySize);
  }

  void ParticleSystemRenderable::FinishUpdate(BufferManager* bufferManager)
  {
    bufferManager->Unmap(bufferIndex_, BufferManager::BUFFER_STAGING_BIT);
    bufferManager->RequestBufferCopy(bufferIndex_, BufferManager::BUFFER_STAGING_BIT);
  }

  void ParticleSystemRenderable::AddToVolume()
  {

  }

  int ParticleSystemRenderable::GetActiveParticles() const
  {
    return particleSystem_->GetAliveCount();
  }

}