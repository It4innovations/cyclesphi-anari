// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "Isosurface.h"
#include "GeometryAttributes.h"
// cycles
#include "scene/mesh.h"
// std
#include <algorithm>
#include <cmath>

namespace anari_cycles {

namespace {

// Marching cubes over the raw samples of a coarse grid yields a visibly
// faceted shell, while the isosurface of the field's trilinear interpolant
// (what the reference devices intersect) is curved. Coarse grids are refined
// by trilinear upsampling -- exactly the field's interpolant -- to at least
// kMinCells cells along the longest axis before extraction.
void refineCoarseGrid(std::vector<float> &voxels,
    anari_vec::uint3 &dims,
    anari_vec::float3 &spacing)
{
  constexpr uint32_t kMinCells = 64;
  constexpr size_t kMaxVoxels = size_t(1) << 24;

  const uint32_t maxCells =
      std::max({dims[0], dims[1], dims[2]}) - 1;
  if (maxCells == 0 || maxCells >= kMinCells)
    return;

  uint32_t f = (kMinCells + maxCells - 1) / maxCells;
  auto refinedSize = [&](uint32_t factor) {
    size_t n = 1;
    for (int a = 0; a < 3; a++)
      n *= size_t(dims[a] - 1) * factor + 1;
    return n;
  };
  while (f > 1 && refinedSize(f) > kMaxVoxels)
    f--;
  if (f <= 1)
    return;

  anari_vec::uint3 rdims;
  for (int a = 0; a < 3; a++)
    rdims[a] = (dims[a] - 1) * f + 1;

  // Per-axis source index and weight of every refined sample
  struct Tap
  {
    uint32_t i0, i1;
    float t;
  };
  std::vector<Tap> taps[3];
  for (int a = 0; a < 3; a++) {
    taps[a].resize(rdims[a]);
    for (uint32_t i = 0; i < rdims[a]; i++) {
      const uint32_t i0 = std::min(i / f, dims[a] - 1);
      taps[a][i] = {i0, std::min(i0 + 1, dims[a] - 1), float(i % f) / float(f)};
    }
  }

  const size_t nx = dims[0], ny = dims[1];
  auto at = [&](uint32_t i, uint32_t j, uint32_t k) {
    return voxels[(size_t(k) * ny + j) * nx + i];
  };
  // Zero-weight taps are skipped so a non-finite neighbor (e.g. outside an
  // unstructured mesh) does not poison samples that do not depend on it.
  auto lerp = [](float a, float b, float t) {
    return t == 0.f ? a : (t == 1.f ? b : a + t * (b - a));
  };

  std::vector<float> refined(size_t(rdims[0]) * rdims[1] * rdims[2]);
  size_t idx = 0;
  for (uint32_t k = 0; k < rdims[2]; k++) {
    const Tap &tz = taps[2][k];
    for (uint32_t j = 0; j < rdims[1]; j++) {
      const Tap &ty = taps[1][j];
      for (uint32_t i = 0; i < rdims[0]; i++) {
        const Tap &tx = taps[0][i];
        auto row = [&](uint32_t jj, uint32_t kk) {
          return lerp(at(tx.i0, jj, kk), at(tx.i1, jj, kk), tx.t);
        };
        auto slab = [&](uint32_t kk) {
          return ty.t == 1.f ? row(ty.i1, kk)
                             : (ty.t == 0.f ? row(ty.i0, kk)
                                            : lerp(row(ty.i0, kk),
                                                  row(ty.i1, kk),
                                                  ty.t));
        };
        refined[idx++] = tz.t == 0.f
            ? slab(tz.i0)
            : lerp(slab(tz.i0), slab(tz.i1), tz.t);
      }
    }
  }

  voxels = std::move(refined);
  dims = rdims;
  for (int a = 0; a < 3; a++)
    spacing[a] /= float(f);
}

} // namespace

Isosurface::Isosurface(CyclesGlobalState *s)
    : Geometry(s), m_field(this), m_isovalueArray(this)
{}

Isosurface::~Isosurface() = default;

void Isosurface::commitParameters()
{
  Geometry::commitParameters();
  commitAttributeParameters();

  m_field = getParamObject<SpatialField>("field");

  m_isovalueArray = getParamObject<Array1D>("isovalue");
  if (m_isovalueArray && m_isovalueArray->elementType() != ANARI_FLOAT32) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "'isovalue' array on isosurface geometry must hold FLOAT32 "
        "(got %s) -- ignoring",
        anari::toString(m_isovalueArray->elementType()));
    m_isovalueArray = nullptr;
  }
  m_isovalue.reset();
  float iso = 0.f;
  if (!m_isovalueArray && getParam("isovalue", ANARI_FLOAT32, &iso))
    m_isovalue = iso;
}

std::vector<float> Isosurface::isovalues() const
{
  if (m_isovalueArray)
    return {m_isovalueArray->beginAs<float>(), m_isovalueArray->endAs<float>()};
  if (m_isovalue)
    return {*m_isovalue};
  return {};
}

void Isosurface::finalize()
{
  if (!m_field) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "missing required parameter 'field' on isosurface geometry");
  }

  std::vector<float> isos = isovalues();
  if (isos.empty()) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "missing required parameter 'isovalue' on isosurface geometry");
  }

  // Re-extract only when the field (its finalize bumps 'lastFinalized' after
  // any data change) or the isovalue set changed; other geometry updates
  // (e.g. attribute arrays) reuse the cached mesh.
  const helium::TimeStamp fieldStamp = m_field ? m_field->lastFinalized() : 0;
  if (m_extractedField != m_field.get() || m_extractedFieldStamp != fieldStamp
      || m_extractedIsovalues != isos) {
    m_mesh = {};
    m_bounds = empty_box3();

    std::vector<float> voxels;
    anari_vec::uint3 dims = {0u, 0u, 0u};
    anari_vec::float3 origin = {0.f, 0.f, 0.f};
    anari_vec::float3 spacing = {1.f, 1.f, 1.f};
    if (m_field && !isos.empty()) {
      if (m_field->getDenseVoxelGrid(voxels, dims, origin, spacing)) {
        refineCoarseGrid(voxels, dims, spacing);
        marchingCubes(voxels.data(),
            dims.data(),
            origin.data(),
            spacing.data(),
            isos,
            m_mesh);
        for (const auto &p : m_mesh.verts)
          extend(m_bounds, make_float3(p[0], p[1], p[2]));
      } else if (m_field->isValid()) {
        reportMessage(ANARI_SEVERITY_WARNING,
            "isosurface geometry: 'field' does not provide dense voxel "
            "access -- extracting nothing");
      }
    }

    m_extractedField = m_field.get();
    m_extractedFieldStamp = fieldStamp;
    m_extractedIsovalues = std::move(isos);
  }

  Geometry::finalize();
}

ccl::Geometry *Isosurface::createCyclesGeometryNode()
{
  return deviceState()->scene->create_node<ccl::Mesh>();
}

void Isosurface::syncCyclesNode(ccl::Geometry *node) const
{
  auto *mesh = (ccl::Mesh *)node;

  // An empty extraction (missing/invalid field or isovalue outside the data
  // range) syncs an empty mesh so a previous extraction cannot linger.
  const size_t numVerts = m_mesh.verts.size();
  const size_t numTris = m_mesh.tris.size() / 3;

  mesh->resize_mesh(numVerts, numTris);
  auto *dstP = mesh->get_position_for_write();
  for (size_t i = 0; i < numVerts; i++) {
    const auto &p = m_mesh.verts[i];
    dstP[i] = make_float3(p[0], p[1], p[2]);
  }
  auto *triangles = mesh->get_triangles().data();
  auto *shader = mesh->get_shader().data();
  auto *smooth = mesh->get_smooth().data();
  for (size_t i = 0; i < m_mesh.tris.size(); i++)
    triangles[i] = int(m_mesh.tris[i]);
  for (size_t t = 0; t < numTris; t++) {
    shader[t] = 0;
    smooth[t] = true; // welded gradient normals shade smoothly
  }
  mesh->tag_triangles_modified();
  mesh->tag_shader_modified();
  mesh->tag_smooth_modified();

  auto &attrs = mesh->attributes;

  // Field-gradient vertex normals
  if (numVerts > 0) {
    Attribute *attr =
        attrs.add(ATTR_STD_VERTEX_NORMAL, ustring("vertex.normal"));
    packed_normal *dst = attr->data_for_write<packed_normal>();
    for (size_t i = 0; i < numVerts; i++) {
      const auto &n = m_mesh.normals[i];
      dst[i] = packed_normal(make_float3(n[0], n[1], n[2]));
    }
    attr->modified = true;
  } else {
    attrs.remove(ATTR_STD_VERTEX_NORMAL);
  }

  // Attribute channels: 'primitive.*' values apply per isovalue shell
  auto primOf = [&](size_t t) -> size_t { return m_mesh.triIsovalue[t]; };

  for (int c = 0; c < NUM_ATTRIBUTE_CHANNELS; c++) {
    if (m_primitiveAttr[c]) {
      writeAttributeArray(
          attrs, c, ATTR_ELEMENT_FACE, numTris, *m_primitiveAttr[c], primOf);
    } else if (m_uniformAttr[c]) {
      writeAttributeConstant(attrs, c, *m_uniformAttr[c]);
    } else if (c == CH_COLOR) {
      writeAttributeConstant(attrs, c, DEFAULT_COLOR);
    } else {
      attrs.remove(ustring(CHANNEL_CYCLES_NAME[c]));
    }
  }

  writePrimitiveId(attrs, ATTR_ELEMENT_FACE, numTris, m_primitiveId.get(), primOf);
}

box3 Isosurface::bounds() const
{
  return m_bounds;
}

} // namespace anari_cycles
