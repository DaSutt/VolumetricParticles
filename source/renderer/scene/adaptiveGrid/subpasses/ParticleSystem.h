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

#include <glm\glm.hpp>
#include <random>

#include "..\..\..\..\scene\Components.h"

namespace Renderer
{
	struct Particle
	{
		glm::vec3 position;
		float radiusSquared;
	};

	class ParticleSystem
	{
	public:
		void SetTransform(const Transform& transform, const glm::vec3& gridMin);
		void SetParticleOffset(int offset);

		void Update(float dt);

		int GetParticleCount() const { return particleCount_; }
		const auto& GetParticles() const { return particles_; }
		const auto& GetRadi() const { return radi_; }
		int GetActiveParticleCount() const { return aliveCount_; }
	private:
		int Spawn(int emitCount);
		void Advect(float dt);
		void Kill();

		glm::vec3 spawnPosition_{};
		float spawnRadius_ = 1.0f;
		int particleCount_ = 40;
		int particleOffset_ = 0;
		float minRadius_ = 0.1f;
		float maxRadius_ = 1.0f;
		float particleLifetime_ = 15.0f;
		float particleSpeed_ = 0.5f;

		const int emissionsPerSecond_ = 5;

		int aliveStart_ = 0;
		int aliveEnd_ = 0;
		int aliveCount_ = 0;

		float emissionsPerFrame_ = 0.0f;

		std::vector<Particle> particles_;
		std::vector<float> lifetimes_;
		std::vector<float> radi_;

		std::vector<int> deadParticles_;

		std::default_random_engine generator_;
		std::uniform_real_distribution<float> spawnDistribution_{ -spawnRadius_, spawnRadius_ };
		std::uniform_real_distribution<float> radiusDistribution_{ minRadius_, maxRadius_ };
	};
}