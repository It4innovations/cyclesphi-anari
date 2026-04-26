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

#include "Triangle.h"

// cycles
#include "scene/mesh.h"

namespace anari_cycles {

Triangle::Triangle(CyclesGlobalState* s)
    : Geometry(s), m_index(this), m_vertexPosition(this)
{}

Triangle::~Triangle() = default;

void Triangle::commitParameters()
{
    Geometry::commitParameters();

    m_index = getParamObject<Array1D>("primitive.index");
    m_vertexPosition = getParamObject<Array1D>("vertex.position");
    m_vertexNormal = getParamObject<Array1D>("vertex.normal");
    m_vertexColor = getParamObject<Array1D>("vertex.color");
    m_vertexAttribute0 = getParamObject<Array1D>("vertex.attribute0");
    m_vertexAttribute1 = getParamObject<Array1D>("vertex.attribute1");
    m_vertexAttribute2 = getParamObject<Array1D>("vertex.attribute2");
    m_vertexAttribute3 = getParamObject<Array1D>("vertex.attribute3");
}

void Triangle::finalize()
{
    if (!m_vertexPosition) {
        reportMessage(ANARI_SEVERITY_WARNING,
            "missing required parameter 'vertex.position' on triangle geometry");
    }

    Geometry::finalize();
}

ccl::Geometry* Triangle::createCyclesGeometryNode()
{
    return deviceState()->scene->create_node<ccl::Mesh>();
}

void Triangle::syncCyclesNode(ccl::Geometry * node) const
{
    auto* mesh = (ccl::Mesh*)node;

    if (!m_vertexPosition) {
        reportMessage(ANARI_SEVERITY_WARNING,
            "Triangle::syncCyclesNode() detected incomplete geometry");
    }

    setVertexPosition(mesh);
    setPrimitiveIndex(mesh);
    setVertexNormal(mesh);
    setVertexColor(mesh);
    setVertexAttribute(mesh, m_vertexAttribute0, "vertex.attribute0");
    setVertexAttribute(mesh, m_vertexAttribute1, "vertex.attribute1");
    setVertexAttribute(mesh, m_vertexAttribute2, "vertex.attribute2");
    setVertexAttribute(mesh, m_vertexAttribute3, "vertex.attribute3");
}

box3 Triangle::bounds() const
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

void Triangle::setVertexPosition(ccl::Mesh * mesh) const
{
    ccl::array<ccl::float3> P;
    auto* dst = P.resize(m_vertexPosition->size());
    std::transform(m_vertexPosition->beginAs<anari_vec::float3>(),
        m_vertexPosition->endAs<anari_vec::float3>(),
        dst,
        [](const anari_vec::float3& v) { return make_float3(v[0], v[1], v[2]); });
    mesh->set_verts(P);
}

void Triangle::setPrimitiveIndex(ccl::Mesh * mesh) const
{
    const uint32_t numTriangles =
        m_index ? m_index->size() : m_vertexPosition->size() / 3;
    mesh->resize_mesh(m_vertexPosition->size(), numTriangles);
    auto& triangles = mesh->get_triangles();
    auto& shader = mesh->get_shader();
    auto& smooth = mesh->get_smooth();
    for (uint32_t i = 0; i < numTriangles; i++) {
        if (m_index) {
            auto* idxs = m_index->beginAs<anari_vec::uint3>();
            triangles[i * 3 + 0] = idxs[i][0];
            triangles[i * 3 + 1] = idxs[i][1];
            triangles[i * 3 + 2] = idxs[i][2];
        }
        else {
            triangles[i * 3 + 0] = 3 * i + 0;
            triangles[i * 3 + 1] = 3 * i + 1;
            triangles[i * 3 + 2] = 3 * i + 2;
        }
        shader[i] = 0;
        smooth[i] = true;
    }
}

void Triangle::setVertexNormal(ccl::Mesh * mesh) const
{
    if (!m_vertexNormal)
        return;

    ustring name = ustring("vertex.normal");
    Attribute* attr = mesh->attributes.add(ATTR_STD_VERTEX_NORMAL, name);
    float3* dst = attr->data_float3();
    std::transform(m_vertexNormal->beginAs<anari_vec::float3>(),
        m_vertexNormal->endAs<anari_vec::float3>(),
        dst,
        [](const anari_vec::float3& v) { return make_float3(v[0], v[1], v[2]); });
}

void Triangle::setVertexColor(ccl::Mesh * mesh) const
{
    auto& array = m_vertexColor;
    if (!array)
        return;

    const void* src = array->data();
    anari::DataType type = array->elementType();

    Attribute* attr = mesh->attributes.add(
        ustring("vertex.color"), TypeColor, ATTR_ELEMENT_VERTEX);
    attr->std = ATTR_STD_VERTEX_COLOR;
    float3* dst = attr->data_float3();
    for (uint32_t i = 0; i < array->size(); i++) {
        auto c = anari::anariTypeInvoke<anari_vec::float4, convert_toFloat4>(
            type, src, i);
        dst[i] = make_float3(c[0], c[1], c[2]);
    }
}

void Triangle::setVertexAttribute(ccl::Mesh * mesh,
    const helium::IntrusivePtr<Array1D> &array,
    const char* name) const
{
    if (!array)
        return;

    anari::DataType type = array->elementType();
    const void* src = array->data();

    Attribute* attr = mesh->attributes.add(ATTR_STD_UV, ustring(name));
    float2* dst = attr->data_float2();
    size_t i = 0;
    std::for_each(dst, dst + m_vertexPosition->size(), [&](float2& v) {
        auto r = anari::anariTypeInvoke<anari_vec::float4, convert_toFloat4>(
            type, src, i);
        v.x = r[0];
        v.y = r[1];
        i++;
        });
}

#if 0
Triangle::Triangle(CyclesGlobalState *d)
    : Geometry(d), m_index(this), m_vertex(this)
{}

Triangle::~Triangle() = default;

void Triangle::commitParameters()
{
  Geometry::commitParameters();
  m_index = getParamObject<Array1D>("primitive.index");
  m_vertex = getParamObject<Array1D>("vertex.position");
  m_vertexNormal = getParamObject<Array1D>("vertex.normal");
  m_vertexNormalFV = getParamObject<Array1D>("faceVarying.normal");
  m_vertexTangent = getParamObject<Array1D>("vertex.tangent");
  m_vertexTangentFV = getParamObject<Array1D>("faceVarying.tangent");
  m_cullBackfaces = getParam<bool>("cullBackfaces", false);
  commitAttributes("vertex.", m_vertexAttributes);
  commitAttributes("faceVarying.", m_vertexAttributesFV);
}

void Triangle::finalize()
{
  if (!m_vertex) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "missing required parameter 'vertex.position' on triangle geometry");
    return;
  }

  if (!m_index && m_vertex->size() % 3 != 0) {
    reportMessage(ANARI_SEVERITY_ERROR,
        "'vertex.position' on triangle geometry is a non-multiple of 3"
        " without 'primitive.index' present");
    return;
  }

  if (m_vertexNormal && m_vertex->size() != m_vertexNormal->size()) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "'vertex.normal' on triangle geometry not the same size as "
        "'vertex.position' (%zu) vs. (%zu)",
        m_vertexNormal->size(),
        m_vertex->size());
  }

  if (m_vertexNormalFV && 3 * m_index->size() != m_vertexNormalFV->size()) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "'faceVarying.normal' on triangle geometry is not matching "
        "the number of triangles in 'primitive.index' (%zu) vs. (%zu)",
        m_vertexNormalFV->size(),
        m_index->size());
  }

  if (m_vertexTangent && m_vertex->size() != m_vertexTangent->size()) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "'vertex.Tangent' on triangle geometry not the same size as "
        "'vertex.position' (%zu) vs. (%zu)",
        m_vertexTangent->size(),
        m_vertex->size());
  }

  if (m_vertexTangentFV && 3 * m_index->size() != m_vertexTangentFV->size()) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "'faceVarying.Tangent' on triangle geometry is not matching "
        "the number of triangles in 'primitive.index' (%zu) vs. (%zu)",
        m_vertexTangentFV->size(),
        m_index->size());
  }

  reportMessage(ANARI_SEVERITY_DEBUG,
      "finalizing %s triangle geometry",
      m_index ? "indexed" : "soup");

  m_vertexBufferPtr = (CUdeviceptr)m_vertex->beginAs<vec3>(AddressSpace::GPU);

  upload();
}

void Triangle::populateBuildInput(OptixBuildInput &buildInput) const
{
  buildInput.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;

  buildInput.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
  buildInput.triangleArray.vertexStrideInBytes = sizeof(vec3);
  buildInput.triangleArray.numVertices = m_vertex->size();
  buildInput.triangleArray.vertexBuffers = &m_vertexBufferPtr;

  if (m_index) {
    buildInput.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
    buildInput.triangleArray.indexStrideInBytes = sizeof(uvec3);
    buildInput.triangleArray.numIndexTriplets = m_index->size();
    buildInput.triangleArray.indexBuffer =
        (CUdeviceptr)m_index->beginAs<uvec3>(AddressSpace::GPU);
  } else {
    buildInput.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_NONE;
    buildInput.triangleArray.indexStrideInBytes = 0;
    buildInput.triangleArray.numIndexTriplets = 0;
    buildInput.triangleArray.indexBuffer = 0;
  }

  static uint32_t buildInputFlags[1] = {0};

  buildInput.triangleArray.flags = buildInputFlags;
  buildInput.triangleArray.numSbtRecords = 1;
}

int Triangle::optixGeometryType() const
{
  return OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
}

bool Triangle::isValid() const
{
  return m_vertex;
}

GeometryGPUData Triangle::gpuData() const
{
  auto retval = Geometry::gpuData();
  retval.type = GeometryType::TRIANGLE;

  auto &tri = retval.tri;
  tri.vertices = m_vertex->beginAs<vec3>(AddressSpace::GPU);
  tri.indices = m_index ? m_index->beginAs<uvec3>(AddressSpace::GPU) : nullptr;
  tri.vertexNormals = m_vertexNormal
      ? m_vertexNormal->beginAs<vec3>(AddressSpace::GPU)
      : nullptr;
  tri.vertexTangents = m_vertexTangent
      ? m_vertexTangent->beginAs<vec4>(AddressSpace::GPU)
      : nullptr;
  populateAttributeDataSet(m_vertexAttributes, tri.vertexAttr);
  populateAttributeDataSet(m_vertexAttributesFV, tri.vertexAttrFV);
  tri.vertexNormalsFV = m_vertexNormalFV
      ? m_vertexNormalFV->beginAs<vec3>(AddressSpace::GPU)
      : nullptr;
  tri.vertexTangentsFV = m_vertexTangentFV
      ? m_vertexTangentFV->beginAs<vec4>(AddressSpace::GPU)
      : nullptr;
  tri.cullBackfaces = m_cullBackfaces;

  return retval;
}
#endif
} // namespace anari_cycles
