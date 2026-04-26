/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "VolumeImageLoader.h"
// anari
#include "anari/anari_cpp.hpp"

namespace anari_cycles {

VolumeImageLoader::VolumeImageLoader(const StructuredRegularField *field_ptr)
    : p_field(field_ptr), VDBImageLoader("")
{}

VolumeImageLoader::~VolumeImageLoader() = default;

void VolumeImageLoader::get_transform(Transform &world_to_index) {
   
    //index_to_object[0].x = p_field->m_spacing[0];
    //index_to_object[0].y = 0;
    //index_to_object[0].z = 0;
    //index_to_object[0].w = p_field->m_origin[0];

    //index_to_object[1].x = 0;
    //index_to_object[1].y = p_field->m_spacing[1];
    //index_to_object[1].z = 0;
    //index_to_object[1].w = p_field->m_origin[1];

    //index_to_object[2].x = 0;
    //index_to_object[2].y = 0;
    //index_to_object[2].z = p_field->m_spacing[2];
    //index_to_object[2].w = p_field->m_origin[2];

// --- world -> index transform ---
// index = (world - origin) / spacing
    auto inv_sx = p_field->m_dims[0] * p_field->m_spacing[0];// 1.f / (p_field->m_dims[0] * p_field->m_spacing[0]);// 1.f / p_field->m_spacing[0];
    auto inv_sy = p_field->m_dims[1] * p_field->m_spacing[1];//1.f / (p_field->m_dims[1] * p_field->m_spacing[1]);//1.f / p_field->m_spacing[1];
    auto inv_sz = p_field->m_dims[2] * p_field->m_spacing[2];//1.f / (p_field->m_dims[2] * p_field->m_spacing[2]);//1.f / p_field->m_spacing[2];

    // If your `origin_` is the CORNER of voxel (0,0,0), keep offset = 0.
    // If your `origin_` is the CENTER of voxel (0,0,0), set half_voxel = +0.5f.
    // Try toggling this to fix a remaining 0.5-voxel shift.
    const float half_voxel = 0.5f;          // use 0.5f if origin is at voxel center

    //Transform world_to_index;

    // Row 0: i =  inv_sx * x + 0*y + 0*z + (-origin.x * inv_sx + half_voxel)
    world_to_index[0].x = inv_sx;
    world_to_index[0].y = 0.0f;
    world_to_index[0].z = 0.0f;
    world_to_index[0].w = p_field->m_origin[0];// -p_field->m_origin[0] * inv_sx + half_voxel * inv_sx;

    // Row 1: j =  0*x + inv_sy * y + 0*z + (-origin.y * inv_sy + half_voxel)
    world_to_index[1].x = 0.0f;
    world_to_index[1].y = inv_sy;
    world_to_index[1].z = 0.0f;
    world_to_index[1].w = p_field->m_origin[1];//-p_field->m_origin[1] * inv_sy + half_voxel * inv_sy;

    // Row 2: k =  0*x + 0*y + inv_sz * z + (-origin.z * inv_sz + half_voxel)
    world_to_index[2].x = 0.0f;
    world_to_index[2].y = 0.0f;
    world_to_index[2].z = inv_sz;
    world_to_index[2].w = p_field->m_origin[2];//-p_field->m_origin[2] * inv_sz + half_voxel * inv_sz;
}

bool VolumeImageLoader::load_metadata(ImageMetaData &metadata)
{  


  // TODO: MJ
  //metadata.transform_3d = //transform_identity();
  //   ccl::transform_scale(ccl::make_float3(1.f / p_field->m_dims[0],
  //       1.f / p_field->m_dims[1],
  //       1.f / p_field->m_dims[2]));

  // TODO: MJ
  Transform index_to_object;
  get_transform(index_to_object);
  //index_to_object = transform_inverse(index_to_object);
  metadata.transform_3d = index_to_object;

  metadata.use_transform_3d = false;

  auto byte_size = p_field->m_data->totalSize()
      * anari::sizeOf(p_field->m_data->elementType());
  metadata.width = byte_size;
  //p_field->m_dims[0];
  metadata.height = 1;
  //p_field->m_dims[1];
  //metadata.depth = p_field->m_dims[2];
#if 0
  metadata.width = p_field->m_dims[0];
  metadata.height = p_field->m_dims[1];
#endif

  switch (p_field->m_data->elementType()) {
  case (ANARI_FLOAT32):
    metadata.type = IMAGE_DATA_TYPE_RAW3D_FLOAT;
    metadata.channels = 1;
    break;
  case (ANARI_FLOAT32_VEC3):
    metadata.type = IMAGE_DATA_TYPE_RAW3D_FLOAT3;
    metadata.channels = 3;
    break;
  default:
    // TODO throw error
    std::cerr << "Unsupported voxel data type\n";
    return false;
  }

  return true;
}

bool VolumeImageLoader::load_pixels(const ImageMetaData &, void *pixels)
{
  auto size = p_field->m_data->totalSize()
      * anari::sizeOf(p_field->m_data->elementType());
  memcpy(pixels, p_field->m_data->data(), size);
  return true;
}

string VolumeImageLoader::name() const
{
  return "ANARI Volume";
}

bool VolumeImageLoader::equals(const ImageLoader &other) const
{
  // TODO
  return false;
}

void VolumeImageLoader::cleanup()
{
  // no-op
}

bool VolumeImageLoader::is_vdb_loader() const
{
  return true;
}

bool VolumeImageLoader::is_simple_mesh() const
{
  return true;
}

void VolumeImageLoader::get_bbox(int3& min_bbox, int3& max_bbox)
{
    min_bbox = make_int3(0, 0, 0);
    max_bbox = make_int3(p_field->m_dims[0] - 1, p_field->m_dims[1] - 1, p_field->m_dims[2] - 1);

    //box3 bounds = p_field->bounds();
    //min_bbox = make_int3(bounds.lower[0], bounds.lower[1], bounds.lower[2]);
    //max_bbox = make_int3(bounds.upper[0], bounds.upper[1], bounds.upper[2]);
}

float3 VolumeImageLoader::index_to_world(float3 index)
{
    return make_float3((float)index[0], (float)index[1], (float)index[2]);

    Transform index_to_object;
    get_transform(index_to_object);
    index_to_object = transform_inverse(index_to_object);

    return ccl::transform_point(&index_to_object, index);

    // Transform from voxel index to world coordinates
    // world_pos = origin + index * spacing
    //return make_float3(
    //    p_field->m_origin[0] + index.x,// * p_field->m_spacing[0],
    //    p_field->m_origin[1] + index.y,// * p_field->m_spacing[1],
    //    p_field->m_origin[2] + index.z// * p_field->m_spacing[2]
    //);
}

} // namespace anari_cycles
