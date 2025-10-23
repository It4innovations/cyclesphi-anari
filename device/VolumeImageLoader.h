/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "SpatialField.h"
// cycles
//#include "scene/image.h"
#include "scene/image_vdb.h"

namespace anari_cycles {

class VolumeImageLoader : public ccl::VDBImageLoader
{
 public:
  VolumeImageLoader(const StructuredRegularField *field_ptr);
  ~VolumeImageLoader();

  virtual bool load_metadata(const ccl::ImageDeviceFeatures &features,
      ccl::ImageMetaData &metadata) override;

  virtual bool load_pixels(const ccl::ImageMetaData &metadata,
      void *pixels,
      const size_t pixels_size,
      const bool associate_alpha) override;

  virtual string name() const override;

  virtual bool equals(const ccl::ImageLoader &other) const override;

  virtual void cleanup() override;

  virtual bool is_vdb_loader() const override;

  virtual bool is_simple_mesh() const override;

  virtual void get_bbox(int3& min_bbox, int3& max_bbox) override;

  virtual float3 index_to_world(float3 in) override;

 protected:
  const StructuredRegularField *p_field;

  void get_transform(Transform& index_to_object);
};

} // namespace anari_cycles
