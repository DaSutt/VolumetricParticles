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

#include "..\..\..\passes\GuiPass.h"

namespace Renderer
{
	void ParticleSystem::SetTransform(const Transform& transform, const glm::vec3& gridMin)
	{
		spawnPosition_ = transform.pos - gridMin;
	}

	void ParticleSystem::SetParticleOffset(int offset)
	{
		particleOffset_ = offset;
	}

	void ParticleSystem::Update(float dt)
	{
		const auto& particleState = GuiPass::GetParticleState();
		particleCount_ = particleState.particleCount;
		spawnRadius_ = particleState.spawnRadius;
		minRadius_ = particleState.minParticleRadius;
		maxRadius_ = particleState.maxParticleRadius;

		particles_.resize(particleCount_);
		radi_.resize(particleCount_);
		lifetimes_.resize(particleCount_);
	
		emissionsPerFrame_ += emissionsPerSecond_ * dt;
		int emitCount = static_cast<int>(floor(emissionsPerFrame_));

		if (emitCount > 0)
		{
			emissionsPerFrame_ -= Spawn(emitCount);
			if (aliveCount_ == particles_.size())
			{
				emissionsPerFrame_ = 0;
			}
		}

		Advect(dt);

		Kill();
	}

	int ParticleSystem::Spawn(int emitCount)
	{
		int count = 0;
		for (int i = 0; i < emitCount; ++i)
		{
			if (aliveCount_ < particles_.size())
			{
				particles_[aliveCount_].position = glm::vec3(spawnDistribution_(generator_), 
					spawnDistribution_(generator_), spawnDistribution_(generator_)) + spawnPosition_;
				radi_[aliveCount_] = 2.0f;
				particles_[aliveCount_].radiusSquared = radi_[aliveCount_] * radi_[aliveCount_];
				lifetimes_[aliveCount_] = 0.0f;
				++aliveCount_;
				++count;
			}
		}
		printf("Alive count %d\n", aliveCount_);
		return count;
	}

	void ParticleSystem::Advect(float dt)
	{
		for (int i = 0; i < aliveCount_; ++i)
		{
			lifetimes_[i] += dt;
			if (lifetimes_[i] > particleLifetime_)
			{
				deadParticles_.push_back(i);
				continue;
			}

			particles_[i].position.y -= dt * particleSpeed_;
		}
	}

	void ParticleSystem::Kill()
	{
		for (auto index : deadParticles_)
		{
			--aliveCount_;
			std::swap(particles_[index], particles_[aliveCount_]);
		}
		deadParticles_.clear();
	}
}