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

#include "Sphere.h"
//// thrust
//#include <thrust/device_ptr.h>
//#include <thrust/sequence.h>
//#include <thrust/transform.h>

// cycles
#include "scene/pointcloud.h"

namespace anari_cycles {

Sphere::Sphere(CyclesGlobalState* s)
    : Geometry(s), m_index(this), m_vertexPosition(this)
{}

Sphere::~Sphere() = default;

void Sphere::commitParameters()
{
    Geometry::commitParameters();

    m_index = getParamObject<Array1D>("primitive.index");
    m_vertexPosition = getParamObject<Array1D>("vertex.position");
    m_vertexColor = getParamObject<Array1D>("vertex.color");
    m_vertexAttribute0 = getParamObject<Array1D>("vertex.attribute0");
    m_vertexAttribute1 = getParamObject<Array1D>("vertex.attribute1");
    m_vertexAttribute2 = getParamObject<Array1D>("vertex.attribute2");
    m_vertexAttribute3 = getParamObject<Array1D>("vertex.attribute3");
    m_vertexRadius = getParamObject<Array1D>("vertex.radius");
    m_radius = getParam<float>("radius", 1.f);
}

void Sphere::finalize()
{
    if (!m_vertexPosition) {
        reportMessage(ANARI_SEVERITY_WARNING,
            "missing required parameter 'vertex.position' on sphere geometry");
    }

    Geometry::finalize();
}

ccl::Geometry* Sphere::createCyclesGeometryNode()
{
    return deviceState()->scene->create_node<ccl::PointCloud>();
}

void Sphere::syncCyclesNode(ccl::Geometry * node) const
{
    if (!m_vertexPosition) {
        reportMessage(ANARI_SEVERITY_WARNING,
            "Spheres::syncCyclesNode() detected incomplete geometry");
    }

    auto* pc = (ccl::PointCloud*)node;
    setSpheres(pc);
    setAttributes(pc);
}

box3 Sphere::bounds() const
{
    box3 b = empty_box3();
    if (!m_vertexPosition)
        return b;
    std::for_each(m_vertexPosition->beginAs<anari_vec::float3>(),
        m_vertexPosition->endAs<anari_vec::float3>(),
        [&](const anari_vec::float3& v) {
            extend(b, make_float3(v[0], v[1], v[2]));
        });
    return b;
}

void Sphere::setSpheres(ccl::PointCloud * pc) const
{
    ccl::array<ccl::float3> points;
    ccl::array<float> radius;
    ccl::array<int> shader;

    size_t numSpheres = m_index ? m_index->size() : m_vertexPosition->size();

    auto* dstPoint = (ccl::float3*)points.resize(numSpheres);
    auto* dstRadius = (float*)radius.resize(numSpheres);
    auto* dstShader = (int*)shader.resize(numSpheres);

    const auto* srcPoint = m_vertexPosition->beginAs<anari_vec::float3>();
    const float* srcRadius = nullptr;
    if (m_vertexRadius)
        srcRadius = m_vertexRadius->beginAs<float>();

    const uint32_t* srcIdx = nullptr;
    if (m_index)
        srcIdx = m_index->beginAs<uint32_t>();

    for (size_t i = 0; i < numSpheres; i++) {
        size_t idx = srcIdx ? size_t(srcIdx[i]) : i;
        const auto& pt = srcPoint[idx];
        dstPoint[i] = make_float3(pt[0], pt[1], pt[2]);
        dstRadius[i] = srcRadius ? srcRadius[idx] : m_radius;
        dstShader[i] = 0;
    }

    pc->set_points(points);
    pc->set_radius(radius);
    pc->set_shader(shader);
}

void Sphere::setAttributes(ccl::PointCloud * pc) const
{
    float3* dstC = nullptr;
    float3* dst0 = nullptr;
    float3* dst1 = nullptr;
    float3* dst2 = nullptr;
    float3* dst3 = nullptr;

    const void* srcC = nullptr;
    const void* src0 = nullptr;
    const void* src1 = nullptr;
    const void* src2 = nullptr;
    const void* src3 = nullptr;

    anari::DataType srcTC = ANARI_UNKNOWN;
    anari::DataType srcT0 = ANARI_UNKNOWN;
    anari::DataType srcT1 = ANARI_UNKNOWN;
    anari::DataType srcT2 = ANARI_UNKNOWN;
    anari::DataType srcT3 = ANARI_UNKNOWN;

    size_t numSpheres = m_index ? m_index->size() : m_vertexPosition->size();

    if (m_vertexColor) {
        Attribute* attr = pc->attributes.add(
            ustring("vertex.color"), TypeColor, ATTR_ELEMENT_VERTEX);
        attr->std = ATTR_STD_VERTEX_COLOR;
        dstC = attr->data_float3();
        srcC = m_vertexColor->data();
        srcTC = m_vertexColor->elementType();
    }

    if (m_vertexAttribute0) {
        Attribute* attr = pc->attributes.add(
            ustring("vertex.attribute0"), TypeColor, ATTR_ELEMENT_VERTEX);
        attr->std = ATTR_STD_VERTEX_COLOR;
        dst0 = attr->data_float3();
        src0 = m_vertexAttribute0->data();
        srcT0 = m_vertexAttribute0->elementType();
    }

    if (m_vertexAttribute1) {
        Attribute* attr = pc->attributes.add(
            ustring("vertex.attribute1"), TypeColor, ATTR_ELEMENT_VERTEX);
        attr->std = ATTR_STD_VERTEX_COLOR;
        dst1 = attr->data_float3();
        src1 = m_vertexAttribute1->data();
        srcT1 = m_vertexAttribute1->elementType();
    }

    if (m_vertexAttribute2) {
        Attribute* attr = pc->attributes.add(
            ustring("vertex.attribute2"), TypeColor, ATTR_ELEMENT_VERTEX);
        attr->std = ATTR_STD_VERTEX_COLOR;
        dst2 = attr->data_float3();
        src2 = m_vertexAttribute2->data();
        srcT2 = m_vertexAttribute2->elementType();
    }

    if (m_vertexAttribute3) {
        Attribute* attr = pc->attributes.add(
            ustring("vertex.attribute3"), TypeColor, ATTR_ELEMENT_VERTEX);
        attr->std = ATTR_STD_VERTEX_COLOR;
        dst3 = attr->data_float3();
        src3 = m_vertexAttribute3->data();
        srcT3 = m_vertexAttribute3->elementType();
    }

    const uint32_t* srcIdx = nullptr;
    if (m_index)
        srcIdx = m_index->beginAs<uint32_t>();
    for (size_t i = 0; i < numSpheres; i++) {
        size_t idx = srcIdx ? size_t(srcIdx[i]) : i;
        if (dstC) {
            auto c = anari::anariTypeInvoke<anari_vec::float4, convert_toFloat4>(
                srcTC, srcC, idx);
            dstC[i] = make_float3(c[0], c[1], c[2]);
        }

        if (dst0) {
            auto c = anari::anariTypeInvoke<anari_vec::float4, convert_toFloat4>(
                srcT0, src0, idx);
            dst0[i] = make_float3(c[0], c[1], c[2]);
        }

        if (dst1) {
            auto c = anari::anariTypeInvoke<anari_vec::float4, convert_toFloat4>(
                srcT1, src1, idx);
            dst1[i] = make_float3(c[0], c[1], c[2]);
        }

        if (dst2) {
            auto c = anari::anariTypeInvoke<anari_vec::float4, convert_toFloat4>(
                srcT2, src2, idx);
            dst2[i] = make_float3(c[0], c[1], c[2]);
        }

        if (dst3) {
            auto c = anari::anariTypeInvoke<anari_vec::float4, convert_toFloat4>(
                srcT3, src3, idx);
            dst3[i] = make_float3(c[0], c[1], c[2]);
        }
    }
}

#if 0
Sphere::Sphere(CyclesGlobalState *d)
    : Geometry(d), m_index(this), m_vertex(this), m_vertexRadius(this)
{}

Sphere::~Sphere() = default;

void Sphere::commitParameters()
{
  Geometry::commitParameters();
  m_index = getParamObject<Array1D>("primitive.index");
  m_vertex = getParamObject<Array1D>("vertex.position");
  m_vertexRadius = getParamObject<Array1D>("vertex.radius");
  m_globalRadius = getParam<float>("radius", 0.01f);
  commitAttributes("vertex.", m_vertexAttributes);
}

void Sphere::finalize()
{
  if (!m_vertex) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "missing required parameter 'vertex.position' on sphere geometry");
    return;
  }

  reportMessage(ANARI_SEVERITY_DEBUG,
      "finalizing %s sphere geometry",
      m_index ? "indexed" : "soup");

  // Calculate bounds //

  m_numSpheres = m_index ? m_index->size() : m_vertex->size();
  m_aabbs.reserve(m_numSpheres * sizeof(box3));

  const float globalRadius = m_globalRadius;
  const float *radii = nullptr;
  if (m_vertexRadius)
    radii = m_vertexRadius->beginAs<float>(AddressSpace::GPU);

  const auto *vertices = m_vertex->beginAs<vec3>(AddressSpace::GPU);

  auto &state = *deviceState();

  if (m_index) {
    auto *begin = m_index->beginAs<uint32_t>(AddressSpace::GPU);
    auto *end = m_index->endAs<uint32_t>(AddressSpace::GPU);
    thrust::transform(thrust::cuda::par.on(state.stream),
        begin,
        end,
        thrust::device_pointer_cast<box3>((box3 *)m_aabbs.ptr()),
        [=] __device__(uint32_t i) {
          const auto &v = vertices[i];
          const float r = radii ? radii[i] : globalRadius;
          return box3(v - r, v + r);
        });
  } else {
    DeviceBuffer index;
    index.reserve(m_numSpheres * sizeof(uint32_t));
    auto idx_begin =
        thrust::device_pointer_cast<uint32_t>((uint32_t *)index.ptr());
    auto idx_end = idx_begin + m_numSpheres;
    thrust::sequence(thrust::cuda::par.on(state.stream), idx_begin, idx_end);
    thrust::transform(thrust::cuda::par.on(state.stream),
        idx_begin,
        idx_end,
        thrust::device_pointer_cast<box3>((box3 *)m_aabbs.ptr()),
        [=] __device__(uint32_t i) {
          const auto &v = vertices[i];
          const float r = radii ? radii[i] : globalRadius;
          return box3(v - r, v + r);
        });
  }

  m_aabbsBufferPtr = (CUdeviceptr)m_aabbs.ptr();

  upload();
}

bool Sphere::isValid() const
{
  return m_vertex;
}

void Sphere::populateBuildInput(OptixBuildInput &buildInput) const
{
  buildInput.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;

  buildInput.customPrimitiveArray.aabbBuffers = &m_aabbsBufferPtr;
  buildInput.customPrimitiveArray.numPrimitives = m_numSpheres;

  static uint32_t buildInputFlags[1] = {OPTIX_GEOMETRY_FLAG_NONE};

  buildInput.customPrimitiveArray.flags = buildInputFlags;
  buildInput.customPrimitiveArray.numSbtRecords = 1;
}

GeometryGPUData Sphere::gpuData() const
{
  auto retval = Geometry::gpuData();
  retval.type = GeometryType::SPHERE;

  auto &sphere = retval.sphere;
  sphere.centers = m_vertex->beginAs<vec3>(AddressSpace::GPU);
  sphere.indices = nullptr;
  if (m_index)
    sphere.indices = m_index->beginAs<uint32_t>(AddressSpace::GPU);
  sphere.radii = nullptr;
  if (m_vertexRadius)
    sphere.radii = m_vertexRadius->beginAs<float>(AddressSpace::GPU);
  sphere.radius = m_globalRadius;
  populateAttributeDataSet(m_vertexAttributes, sphere.vertexAttr);

  return retval;
}

int Sphere::optixGeometryType() const
{
  return OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
}
#endif

} // namespace anari_cycles
