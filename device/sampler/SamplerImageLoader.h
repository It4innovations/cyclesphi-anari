/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "array/Array1D.h"
#include "array/Array2D.h"
#include "array/Array3D.h"
#include "cycles_math.h"
// helium
#include "helium/utility/IntrusivePtr.h"
// cycles
#include "scene/image.h"

namespace anari_cycles {

// Per-axis wrap mode of an image3D sampler: helium's WrapMode plus the
// clampToBorder mode (sampling outside the image yields 'borderColor').
enum class SamplerWrap
{
  CLAMP_TO_EDGE,
  CLAMP_TO_BORDER,
  REPEAT,
  MIRROR_REPEAT
};

inline SamplerWrap samplerWrapFromString(const std::string &str)
{
  if (str == "repeat")
    return SamplerWrap::REPEAT;
  if (str == "mirrorRepeat")
    return SamplerWrap::MIRROR_REPEAT;
  if (str == "clampToBorder")
    return SamplerWrap::CLAMP_TO_BORDER;
  return SamplerWrap::CLAMP_TO_EDGE;
}

// Layout of a 3D image flattened into a 2D Cycles image (Cycles has no dense
// 3D image textures usable from surface shaders): every Z slice is padded by
// one texel on each side (and one padding slice is added below and above),
// the padding holding what the wrap mode of that axis samples just outside
// the image (the opposite edge for repeat, the edge itself for
// clampToEdge/mirrorRepeat, the border color for clampToBorder). The padded
// slices are tiled tilesX x tilesY into the atlas. With the in-tile
// coordinates kept within the padded tile, bilinear filtering never bleeds
// into a neighboring tile and reproduces the wrap mode exactly at the edges.
struct Atlas3DLayout
{
  uint32_t tilesX{1};
  uint32_t tilesY{1};
  SamplerWrap wrap[3]{SamplerWrap::CLAMP_TO_EDGE,
      SamplerWrap::CLAMP_TO_EDGE,
      SamplerWrap::CLAMP_TO_EDGE};
  float borderColor[4]{0.f, 0.f, 0.f, 0.f};

  bool operator==(const Atlas3DLayout &o) const
  {
    return tilesX == o.tilesX && tilesY == o.tilesY && wrap[0] == o.wrap[0]
        && wrap[1] == o.wrap[1] && wrap[2] == o.wrap[2]
        && borderColor[0] == o.borderColor[0]
        && borderColor[1] == o.borderColor[1]
        && borderColor[2] == o.borderColor[2]
        && borderColor[3] == o.borderColor[3];
  }
};

class SamplerImageLoader : public ccl::ImageLoader
{
 public:
  SamplerImageLoader(Array1D *array);
  SamplerImageLoader(Array2D *array);
  // 3D image flattened into a padded 2D slice atlas (see Atlas3DLayout)
  SamplerImageLoader(Array3D *array, const Atlas3DLayout &layout);
  ~SamplerImageLoader();

  virtual bool load_metadata(ccl::ImageMetaData &metadata,
      const ccl::ImageLoaderParams &params,
      ccl::Progress &progress) override;
  virtual bool load_pixels(
      const ccl::ImageMetaData &metadata, void *pixels) override;
  virtual ccl::string name() const override;
  virtual bool equals(const ccl::ImageLoader &other) const override;
  virtual void cleanup() override;
  virtual bool is_vdb_loader() const override;

 private:
  // Expand all source texels to the 4-component storage layout.
  void expandPixels(ccl::ImageDataType cyclesType, void *dst) const;
  // Scatter the expanded 3D texels (plus wrap padding) into the atlas.
  void fillAtlas(ccl::ImageDataType cyclesType, void *pixels) const;

  // The loader can outlive the sampler that created it (it is owned by the
  // Cycles image slot, and ImageManager::add_image() dedupes new images
  // against every live slot via equals()). Pin the source array so the
  // pointer identity that equals() relies on stays valid -- otherwise a
  // freed array reallocated at the same address dedupes onto a stale image.
  helium::IntrusivePtr<Array1D> m_array1d;
  helium::IntrusivePtr<Array2D> m_array2d;
  helium::IntrusivePtr<Array3D> m_array3d;
  Atlas3DLayout m_layout;
  // Data stamp of the source array: an array whose contents changed in place
  // (same pointer) must not dedupe onto the image built from its old data.
  helium::TimeStamp m_dataStamp{0};

  anari::DataType m_dataType{ANARI_UNKNOWN};
  uint3 m_dims{1, 1, 1}; // source image size
  const void *m_pixels{nullptr};
};

} // namespace anari_cycles
