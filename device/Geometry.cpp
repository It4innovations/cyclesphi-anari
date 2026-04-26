// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "Geometry.h"
// specific types
#include "Cone.h"
#include "Curve.h"
#include "Cylinder.h"
#include "Quad.h"
#include "Sphere.h"
#include "Triangle.h"
// cycles
#include "scene/mesh.h"
#include "scene/pointcloud.h"

namespace anari_cycles {

// Shared mesh attribute helpers ///////////////////////////////////////////////

static void setMeshVertexNormal(
    ccl::Mesh *mesh, const helium::IntrusivePtr<Array1D> &array)
{
  if (!array)
    return;

  ustring name = ustring("vertex.normal");
  Attribute *attr = mesh->attributes.add(ATTR_STD_VERTEX_NORMAL, name);
  float3 *dst = attr->data_float3();
  std::transform(array->beginAs<anari_vec::float3>(),
      array->endAs<anari_vec::float3>(),
      dst,
      [](const anari_vec::float3 &v) {
        return make_float3(v[0], v[1], v[2]);
      });
}

static void setMeshVertexColor(
    ccl::Mesh *mesh, const helium::IntrusivePtr<Array1D> &array)
{
  if (!array)
    return;

  const void *src = array->data();
  anari::DataType type = array->elementType();

  Attribute *attr = mesh->attributes.add(
      ustring("vertex.color"), ccl::TypeColor, ATTR_ELEMENT_VERTEX);
  attr->std = ATTR_STD_VERTEX_COLOR;
  float3 *dst = attr->data_float3();
  for (uint32_t i = 0; i < array->size(); i++) {
    auto c = anari::anariTypeInvoke<anari_vec::float4, convert_toFloat4>(
        type, src, i);
    dst[i] = make_float3(c[0], c[1], c[2]);
  }
}

static void setMeshVertexAttribute(ccl::Mesh *mesh,
    const helium::IntrusivePtr<Array1D> &array,
    const char *name)
{
  if (!array)
    return;

  anari::DataType type = array->elementType();
  const void *src = array->data();

  Attribute *attr =
      mesh->attributes.add(ustring(name), ccl::TypeFloat4, ATTR_ELEMENT_VERTEX);
  float4 *dst = attr->data_float4();
  for (size_t i = 0; i < array->size(); i++) {
    auto r = anari::anariTypeInvoke<anari_vec::float4, convert_toFloat4>(
        type, src, i);
    dst[i].x = r[0];
    dst[i].y = r[1];
    dst[i].z = r[2];
    dst[i].w = r[3];
  }
}

///////////////////////////////////////////////////////////////////////////////
// Geometry definitions ///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Geometry::Geometry(CyclesGlobalState *s) : Object(ANARI_GEOMETRY, s) {}

Geometry::~Geometry() = default;

Geometry *Geometry::createInstance(std::string_view type, CyclesGlobalState *s)
{

    if (type == "triangle")
        return new Triangle(s);
    else if (type == "quad")
        return new Quad(s);
    else if (type == "sphere")
        return new Sphere(s);
    else if (type == "cylinder")
        return new Cylinder(s);
    else if (type == "cone")
        return new Cone(s);
    else if (type == "curve")
        return new Curve(s);
    else
        return (Geometry *)new UnknownObject(ANARI_GEOMETRY, type, s);
}

void Geometry::finalize()
{
  Object::finalize();
}

void Geometry::commitParameters()
{
    commitAttributes("primitive.", m_primitiveAttributes);

    auto getUniformAttribute =
        [&](const std::string& pName) -> std::optional<anari_vec::vec4> {
        anari_vec::vec4 v = { 0.f, 0.f, 0.f, 1.f };
        if (getParam(pName, ANARI_FLOAT32_VEC4, &v))
            return v;
        else
            return {};
    };

    m_uniformAttributes.attribute0 = getUniformAttribute("attribute0");
    m_uniformAttributes.attribute1 = getUniformAttribute("attribute1");
    m_uniformAttributes.attribute2 = getUniformAttribute("attribute2");
    m_uniformAttributes.attribute3 = getUniformAttribute("attribute3");
    m_uniformAttributes.color = getUniformAttribute("color");
    m_primitiveId = getParamObject<Array1D>("primitive.id");
}

void Geometry::markFinalized()
{
    Object::markFinalized();
    //deviceState()->objectUpdates.lastBLASChange = helium::newTimeStamp();
}

void Geometry::commitAttributes(const char* _prefix, GeometryAttributes& attrs)
{
    std::string prefix = _prefix;
    attrs.attribute0 = getParamObject<Array1D>(prefix + "attribute0");
    attrs.attribute1 = getParamObject<Array1D>(prefix + "attribute1");
    attrs.attribute2 = getParamObject<Array1D>(prefix + "attribute2");
    attrs.attribute3 = getParamObject<Array1D>(prefix + "attribute3");
    attrs.color = getParamObject<Array1D>(prefix + "color");
}

//void Geometry::populateAttributeDataSet(
//    const GeometryAttributes& hostAttrs, AttributeDataSet& gpuAttrs) const
//{
//    populateAttributeData(hostAttrs.attribute0, gpuAttrs[0]);
//    populateAttributeData(hostAttrs.attribute1, gpuAttrs[1]);
//    populateAttributeData(hostAttrs.attribute2, gpuAttrs[2]);
//    populateAttributeData(hostAttrs.attribute3, gpuAttrs[3]);
//    populateAttributeData(hostAttrs.color, gpuAttrs[4]);
//}

} // namespace anari_cycles
