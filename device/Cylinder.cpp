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

#include "Cylinder.h"

// cycles
#include "scene/hair.h"

namespace anari_cycles {

Cylinder::Cylinder(CyclesGlobalState *d)
    : Geometry(d), m_index(this), m_radius(this), m_vertex(this)
{}

Cylinder::~Cylinder() = default;

void Cylinder::commitParameters()
{
  Geometry::commitParameters();
  m_index = getParamObject<Array1D>("primitive.index");
  m_radius = getParamObject<Array1D>("primitive.radius");
  m_caps = getParamString("caps", "none") != "none";
  m_vertex = getParamObject<Array1D>("vertex.position");
  m_globalRadius = getParam<float>("radius", 1.f);
  commitAttributes("vertex.", m_vertexAttributes);
}

void Cylinder::finalize()
{
  if (!m_vertex) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "missing required parameter 'vertex.position' on cylinders geometry");
    return;
  }

  reportMessage(ANARI_SEVERITY_DEBUG,
      "finalizing %s cylinder geometry",
      m_index ? "indexed" : "soup");

  //std::vector<uvec2> implicitIndices;
  //Span<uvec2> indices;

  //if (!m_index) {
  //  implicitIndices.resize(m_vertex->size() / 2);
  //  uvec2 idx(0, 1);
  //  std::for_each(
  //      implicitIndices.begin(), implicitIndices.end(), [&](uvec2 &i) {
  //        i = idx;
  //        idx += 2;
  //      });
  //  indices = make_Span(implicitIndices.data(), implicitIndices.size());
  //} else {
  //  indices = make_Span(m_index->beginAs<uvec2>(), m_index->size());
  //}

  //const float *radius = nullptr;
  //if (m_radius)
  //  radius = m_radius->beginAs<float>();

  //m_aabbs.resize(indices.size());

  //const auto *posBegin = m_vertex->beginAs<vec3>();
  //size_t cylinderID = 0;
  //std::transform(
  //    indices.begin(), indices.end(), m_aabbs.begin(), [&](const uvec2 &c) {
  //      const float r =
  //          std::abs(radius ? radius[cylinderID++] : m_globalRadius);
  //      const vec3 &v0 = posBegin[c.x];
  //      const vec3 &v1 = posBegin[c.y];
  //      return box3(glm::min(v0, v1) - r, glm::max(v0, v1) + r);
  //    });

  //m_aabbs.upload();
  //m_aabbsBufferPtr = (CUdeviceptr)m_aabbs.dataDevice();

  //upload();
}

bool Cylinder::isValid() const
{
  return m_vertex;
}

//void Cylinder::populateBuildInput(OptixBuildInput &buildInput) const
//{
//  buildInput.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
//
//  buildInput.customPrimitiveArray.aabbBuffers = &m_aabbsBufferPtr;
//  buildInput.customPrimitiveArray.numPrimitives = m_aabbs.size();
//
//  static uint32_t buildInputFlags[1] = {OPTIX_GEOMETRY_FLAG_NONE};
//
//  buildInput.customPrimitiveArray.flags = buildInputFlags;
//  buildInput.customPrimitiveArray.numSbtRecords = 1;
//}
//
//GeometryGPUData Cylinder::gpuData() const
//{
//  auto retval = Geometry::gpuData();
//  retval.type = GeometryType::CYLINDER;
//
//  auto &cylinder = retval.cylinder;
//  cylinder.vertices = m_vertex->beginAs<vec3>(AddressSpace::GPU);
//  cylinder.indices =
//      m_index ? m_index->beginAs<uvec2>(AddressSpace::GPU) : nullptr;
//  cylinder.radii =
//      m_radius ? m_radius->beginAs<float>(AddressSpace::GPU) : nullptr;
//  cylinder.radius = m_globalRadius;
//  cylinder.caps = m_caps;
//  populateAttributeDataSet(m_vertexAttributes, cylinder.vertexAttr);
//
//  return retval;
//}
//
//int Cylinder::optixGeometryType() const
//{
//  return OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
//}

ccl::Geometry* Cylinder::createCyclesGeometryNode() { 
    return deviceState()->scene->create_node<ccl::Hair>();
}

void Cylinder::syncCyclesNode(ccl::Geometry* node) const {
    if (!m_vertex) {
        reportMessage(ANARI_SEVERITY_WARNING,
            "missing required parameter 'vertex.position' on cylinders geometry");
        return;
    }

    auto* hair = (ccl::Hair*)node;

    //hair->curve_shape = CURVE_RIBBON;   // or CURVE_THICK for round tubes
    //hair->resolution(1);
    //hair->curve_subdivisions(0);

    setCylinders(hair);
    setAttributes(hair);
}

box3 Cylinder::bounds() const { 
    box3 b = empty_box3();
    if (!m_vertex)
        return b;
    std::for_each(m_vertex->beginAs<anari_vec::float3>(),
        m_vertex->endAs<anari_vec::float3>(),
        [&](const anari_vec::float3& v) {
            extend(b, make_float3(v[0], v[1], v[2]));
        });
    return b;
}

void Cylinder::setCylinders(ccl::Hair* hair) const
{
    vector<uint2> implicitIndices;
    vector<uint2> indices;

    if (!m_index) {
      implicitIndices.resize(m_vertex->size() / 2);
      uint2 idx = { 0, 1 };
      std::for_each(
          implicitIndices.begin(), implicitIndices.end(), [&](uint2 &i) {
            i = idx;
            idx = make_uint2(idx[0] + 2, idx[1] + 2);
          });
      indices = std::move(implicitIndices);
    } else {
      indices.resize(m_index->size());
      memcpy(indices.data(), m_index->beginAs<anari_vec::uvec2>(), m_index->size() * sizeof(anari_vec::uvec2));
    }

    const float *radius = nullptr;
    if (m_radius)
      radius = m_radius->beginAs<float>();

    //m_aabbs.resize(indices.size());
    hair->resize_curves(indices.size(), indices.size() * 2);
    hair->curve_shape = CURVE_RIBBON;// CURVE_THICK; //CURVE_RIBBON

    const auto *posBegin = m_vertex->beginAs<anari_vec::vec3>();
    size_t cylinderID = 0;
    //std::transform(
    //    indices.begin(), indices.end(), m_aabbs.begin(), [&](const uvec2 &c) {
    //      const float r =
    //          std::abs(radius ? radius[cylinderID++] : m_globalRadius);
    //      const vec3 &v0 = posBegin[c.x];
    //      const vec3 &v1 = posBegin[c.y];
    //      return box3(glm::min(v0, v1) - r, glm::max(v0, v1) + r);
    //    });

    auto& curve_keys = hair->get_curve_keys();
    auto& curve_radius = hair->get_curve_radius();
    auto& curve_first_key = hair->get_curve_first_key();
    auto& curve_shader = hair->get_curve_shader();
    
    std::for_each(
        indices.begin(), indices.end(), [&](const uint2& c) {
            const float r =
                std::abs(radius ? radius[cylinderID] : m_globalRadius);
            const anari_vec::vec3 &v0 = posBegin[c.x];
            const anari_vec::vec3 &v1 = posBegin[c.y];

            curve_keys[cylinderID * 2 + 0] = make_float3(v0[0], v0[1], v0[2]);
            curve_keys[cylinderID * 2 + 1] = make_float3(v1[0], v1[1], v1[2]);
            curve_radius[cylinderID * 2 + 0] = r;
            curve_radius[cylinderID * 2 + 1] = r;

            curve_first_key[cylinderID] = cylinderID * 2;
            curve_shader[cylinderID] = 0;

            cylinderID++;
        });
}

void Cylinder::setAttributes(ccl::Hair* hair) const
{
    //TOD:MJ
}

} // namespace anari_cycles
