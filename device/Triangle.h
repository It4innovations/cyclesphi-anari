/*
 * Copyright (c) 2019-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <helium/utility/IntrusivePtr.h>
#include "Geometry.h"
#include "Array.h"

namespace anari_cycles {
// Triangle definitions ///////////////////////////////////////////////////////

struct Triangle : public Geometry
{
    Triangle(CyclesGlobalState* s);
    ~Triangle() override;

    void commitParameters() override;
    void finalize() override;

    ccl::Geometry* createCyclesGeometryNode() override;
    void syncCyclesNode(ccl::Geometry* node) const override;

    box3 bounds() const override;

private:
    void setVertexPosition(ccl::Mesh* mesh) const;
    void setPrimitiveIndex(ccl::Mesh* mesh) const;
    void setVertexNormal(ccl::Mesh* mesh) const;
    void setVertexColor(ccl::Mesh* mesh) const;
    void setVertexAttribute(ccl::Mesh* mesh,
        const helium::IntrusivePtr<Array1D>& array,
        const char* name) const;

    helium::ChangeObserverPtr<Array1D> m_index;
    helium::ChangeObserverPtr<Array1D> m_vertexPosition;
    helium::IntrusivePtr<Array1D> m_vertexNormal;
    helium::IntrusivePtr<Array1D> m_vertexColor;
    helium::IntrusivePtr<Array1D> m_vertexAttribute0;
    helium::IntrusivePtr<Array1D> m_vertexAttribute1;
    helium::IntrusivePtr<Array1D> m_vertexAttribute2;
    helium::IntrusivePtr<Array1D> m_vertexAttribute3;
};

#if 0
struct Triangle : public Geometry
{
  Triangle(CyclesGlobalState *d);
  ~Triangle() override;

  void commitParameters() override;
  void finalize() override;
  bool isValid() const override;

  void populateBuildInput(OptixBuildInput &) const override;

  int optixGeometryType() const override;

 private:
  GeometryGPUData gpuData() const override;
  void cleanup();

  helium::ChangeObserverPtr<Array1D> m_index;
  helium::ChangeObserverPtr<Array1D> m_vertex;
  helium::IntrusivePtr<Array1D> m_vertexNormal;
  GeometryAttributes m_vertexAttributes;
  GeometryAttributes m_vertexAttributesFV;
  helium::IntrusivePtr<Array1D> m_vertexNormalFV;
  helium::IntrusivePtr<Array1D> m_vertexTangent;
  helium::IntrusivePtr<Array1D> m_vertexTangentFV;

  CUdeviceptr m_vertexBufferPtr{};

  bool m_cullBackfaces{false};
};
#endif
} // namespace anari_cycles
