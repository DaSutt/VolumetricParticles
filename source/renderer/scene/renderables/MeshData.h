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

#include <vector>
#include <array>

namespace Renderer
{
  struct MeshData
  {
    enum Buffers
    {
      BUFFER_WORLD_VIEW_PROJ,
      BUFFER_WORLD_INV_TRANSPOSE,
      BUFFER_WORLD_VIEW_PROJ_LIGHT,
      BUFFER_MAX
    };

    enum VertexBuffers
    {
      BUFFER_VERTEX_POS,
      BUFFER_VERTEX_NORMAL,
      BUFFER_VERTEX_UV,
      BUFFER_VERTEX_MAX
    };

    static const int elementSizes[BUFFER_VERTEX_MAX];

    std::vector<uint32_t> indexCounts_;
    std::vector<uint32_t> indexOffsets_;
    std::vector<uint32_t> vertexOffsets_;
    std::vector<int> materialIndices_;

    std::vector<int> vertexBufferIndices_;
    int indexBufferIndex_ = -1;

    int vertexCount_ = 0;
    int indexCount_ = 0;
    int meshCount_ = 0;

    std::array<int, BUFFER_MAX> buffers_;
  };
}