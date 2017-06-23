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

#include "ParticleSystem.h"

#include <random>
#include <functional>
#include <limits>

ParticleSystem::ParticleSystem(int particleCount, int emissionsPerSecond, float particleLifetime) :
  particleCount_{particleCount},
  emissionsPerSecond_{emissionsPerSecond},
  particleLifetime_{particleLifetime}
{
  positions_.resize(particleCount, glm::vec3(0.0f));
  lifetime_.resize(particleCount, 0.0f);
}

void ParticleSystem::Update(float dt)
{
  emissionsPerFrame_ += emissionsPerSecond_ * dt;
  int emitCount = static_cast<int>(floor(emissionsPerFrame_));

  if (emitCount > 0)
  {
    emissionsPerFrame_ -= Spawn(emitCount);
  }

  boundingBox_.min = glm::vec3(FLT_MAX);
  boundingBox_.max = glm::vec3(-FLT_MAX);

  Advect(dt);

  Kill();

  if (aliveCount_ <= 0)
  {
    boundingBox_.min = glm::vec3(0.0f);
    boundingBox_.max = glm::vec3(0.0f);
  }
  else
  {
    static const glm::vec3 radius = glm::vec3(particleRadius_, particleRadius_, particleRadius_);
    boundingBox_.max += radius;
    boundingBox_.min -= radius;
  }
}

void ParticleSystem::ApplyTransform(const Transform& transform)
{
  origin_ += transform.pos;
}

int ParticleSystem::Spawn(int emitCount)
{
  static std::default_random_engine generator;
  static std::uniform_real_distribution<float> distribution(-emitRadius_, emitRadius_);
  static auto RandPos = std::bind(distribution, generator);

  int count = 0;
  for (int i = 0; i < emitCount; ++i)
  {
    if (aliveCount_ < positions_.size())
    {
      positions_[aliveCount_] = origin_ + glm::vec3(RandPos(), RandPos(), RandPos());
      lifetime_[aliveCount_] = 0.0f;
      ++aliveCount_;
      ++count;
    }
  }
  return count;
}

void ParticleSystem::Advect(float dt)
{
  for (int i = 0; i < aliveCount_; ++i)
  {
    lifetime_[i] += dt;
    if (lifetime_[i] > particleLifetime_)
    {
      deadParticles_.push_back(i);
      continue;
    }

    static std::default_random_engine generator;
    static std::uniform_real_distribution<float> distribution(0.02f, 0.02f);
    static auto RandPos = std::bind(distribution, generator);

    positions_[i].y -= dt * RandPos() + 0.02f;
    positions_[i].x += dt * RandPos();
    positions_[i].z += dt * RandPos();

    UpdateBounds(positions_[i]);
  }
}

void ParticleSystem::Kill()
{
  for (auto index : deadParticles_)
  {
    --aliveCount_;
    std::swap(positions_[index], positions_[aliveCount_]);
  }
  deadParticles_.clear();
}

void ParticleSystem::UpdateBounds(const glm::vec3 pos)
{
  boundingBox_.min = glm::min(pos, boundingBox_.min);
  boundingBox_.max = glm::max(pos, boundingBox_.max);
}

void ParticleSystem::CheckBounds()
{
  boundingBox_.min = glm::clamp(boundingBox_.min, boundingBox_.max, boundingBox_.min);
  boundingBox_.max = glm::clamp(boundingBox_.max, boundingBox_.max, boundingBox_.min);
}