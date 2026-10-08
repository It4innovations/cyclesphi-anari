// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Sampler.h"
#include "array/Array3D.h"

namespace anari_cycles {

// image3D sampler (KHR_SAMPLER_IMAGE3D). Cycles surface shaders cannot sample
// dense 3D images (voxel images are only reachable through volume
// attributes), so the image is uploaded as a 2D atlas of padded Z slices (see
// Atlas3DLayout) and sampled by shader-graph math: two bilinear slice lookups
// mixed along Z for linear filtering, a single closest lookup for nearest.
// This works identically with SVM on CPU, CUDA and OptiX (and with OSL).
struct Image3D : public Sampler
{
  Image3D(CyclesGlobalState *d);

  bool isValid() const override;
  void commitParameters() override;
  void finalize() override;

  SamplerOutputs createNodeGraph(ccl::ShaderGraph *graph) override;

 private:
  // Observed so committing a change on the array re-finalizes this sampler.
  helium::ChangeObserverPtr<Array3D> m_image;
  SamplerWrap m_wrapMode[3]{SamplerWrap::CLAMP_TO_EDGE,
      SamplerWrap::CLAMP_TO_EDGE,
      SamplerWrap::CLAMP_TO_EDGE};
  helium::float4 m_borderColor{0.f, 0.f, 0.f, 0.f};
  bool m_linearFilter{true};

  // Layout of the uploaded atlas (valid when m_handle is set)
  uint32_t m_dims[3]{0, 0, 0};
  Atlas3DLayout m_layout;
};

} // namespace anari_cycles
