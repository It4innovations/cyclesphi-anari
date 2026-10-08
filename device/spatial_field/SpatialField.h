// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "array/Array1D.h"
#include "array/Array3D.h"
#include "Object.h"
// cycles
#include "scene/image.h"
#include "scene/shader_graph.h"
// std
#include <vector>

namespace ccl {
class Geometry;
}

namespace anari_cycles {

struct SpatialField : public Object
{
  SpatialField(CyclesGlobalState *s);
  ~SpatialField() override;

  static SpatialField *createInstance(
      std::string_view subtype, CyclesGlobalState *s);

  void finalize() override;

  // Add shader nodes to 'graph' that sample this field at the current
  // (object space) shading position. Returns the scalar field value output,
  // or nullptr if sampling nodes cannot be created.
  virtual ccl::ShaderOutput *createCyclesSamplingNodes(
      ccl::ShaderGraph *graph) = 0;

  // Optional coverage in [0,1] of the field's domain at the shading position
  // (fields that do not fill their bounding box, e.g. unstructured meshes,
  // are 0 outside their cells). Consumers scale the extinction by it. Returns
  // nullptr when the field covers its whole bounds.
  virtual ccl::ShaderOutput *createCyclesCoverageNodes(ccl::ShaderGraph *)
  {
    return nullptr;
  }

  // Fields sampled through Cycles' native voxel-grid (VDB) path store their
  // grid image in m_voxelImage; the consuming volume re-attaches it to its
  // geometry here on every finalize (the geometry was just rebuilt, which
  // drops previously attached attributes). Filtering — including tricubic —
  // is carried per grid image (ImageParams::interpolation), so fields with
  // different 'filter' settings can share one volume shader.
  void attachVoxelAttributes(ccl::Geometry *geom) const;

  virtual box3 bounds() const = 0;

  // Suggested object-space ray marching step size.
  virtual float stepSize() const = 0;

  // Dense scalar grid access for isosurface extraction: fills 'dims',
  // 'origin' and 'spacing' and writes dims.x*dims.y*dims.z voxel values
  // (normalized to float, x fastest) into 'voxels'. Returns false when this
  // field cannot provide a dense grid (the isosurface geometry then extracts
  // an empty mesh).
  virtual bool getDenseVoxelGrid(std::vector<float> &voxels,
      anari_vec::uint3 &dims,
      anari_vec::float3 &origin,
      anari_vec::float3 &spacing) const
  {
    return false;
  }

 protected:
  // Shader-graph side of the voxel-grid path: an AttributeNode bound to
  // m_voxelAttributeName (the kernel samples the attached grid image at the
  // shading position with the image's own interpolation mode).
  ccl::ShaderOutput *createVoxelSamplingNodes(ccl::ShaderGraph *graph);
  // Same for the optional coverage grid image (m_coverageImage).
  ccl::ShaderOutput *createVoxelCoverageNodes(ccl::ShaderGraph *graph);

  // Grid image sampled through the voxel attribute (empty when this field
  // does not use the VDB path).
  ccl::ImageHandle m_voxelImage;
  // Optional second grid image (coverage of the field's domain), attached
  // alongside m_voxelImage under its own attribute name.
  ccl::ImageHandle m_coverageImage;

 private:
  // Unique per-instance attribute name tying createVoxelSamplingNodes'
  // AttributeNode to the image attached by attachVoxelAttributes (a volume
  // shader can sample several fields, e.g. principled density+temperature).
  ccl::ustring m_voxelAttributeName;
  ccl::ustring m_coverageAttributeName;
};

} // namespace anari_cycles

CYCLES_ANARI_TYPEFOR_SPECIALIZATION(
    anari_cycles::SpatialField *, ANARI_SPATIAL_FIELD);
