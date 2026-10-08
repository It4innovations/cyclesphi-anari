// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "SpatialField.h"
#include "array/Array1D.h"

namespace anari_cycles {

// 'unstructured' spatial field (KHR_SPATIAL_FIELD_UNSTRUCTURED): tetrahedra,
// hexahedra, wedges and pyramids (VTK vertex ordering and cell type codes)
// with vertex- or cell-centered scalars. Cycles has no unstructured volume
// primitive, so the cells are resampled on the CPU onto a regular grid over
// their bounds (vertex-centered data with the cells' isoparametric
// interpolation) and sampled through the same native voxel-grid path as
// 'structuredRegular' fields. A second grid holds the coverage of the cells
// (1 inside, 0 outside, the covered fraction of voxels straddling the mesh
// boundary, which keeps edges anti-aliased): the value grid stores
// value * coverage so that
// value = sampled / coverage is not diluted at cell boundaries, and volumes
// scale their extinction by the coverage so space between cells stays empty.
struct UnstructuredField : public SpatialField
{
  UnstructuredField(CyclesGlobalState *s);
  ~UnstructuredField() override;

  void commitParameters() override;
  void finalize() override;

  ccl::ShaderOutput *createCyclesSamplingNodes(ccl::ShaderGraph *graph) override;
  ccl::ShaderOutput *createCyclesCoverageNodes(ccl::ShaderGraph *graph) override;

  box3 bounds() const override;
  float stepSize() const override;
  bool isValid() const override;

  // Resampled grid; voxels outside all cells are NaN (marching cubes skips
  // them, so isosurfaces end at the mesh boundary).
  bool getDenseVoxelGrid(std::vector<float> &voxels,
      anari_vec::uint3 &dims,
      anari_vec::float3 &origin,
      anari_vec::float3 &spacing) const override;

 private:
  bool resample();

  helium::ChangeObserverPtr<Array1D> m_vertexPosition;
  helium::ChangeObserverPtr<Array1D> m_vertexData;
  helium::ChangeObserverPtr<Array1D> m_index;
  helium::ChangeObserverPtr<Array1D> m_cellData;
  helium::ChangeObserverPtr<Array1D> m_cellType;
  helium::ChangeObserverPtr<Array1D> m_cellIndex;
  // Device specific: voxel count along the longest axis of the resampling
  // grid (0 = automatic, from the cell count).
  int m_resolution{0};

  // Resampling result
  bool m_valid{false};
  anari_vec::uint3 m_dims{0u, 0u, 0u};
  anari_vec::float3 m_origin{0.f, 0.f, 0.f};
  anari_vec::float3 m_spacing{1.f, 1.f, 1.f};
  std::vector<float> m_values; // NaN outside the cells
};

} // namespace anari_cycles
