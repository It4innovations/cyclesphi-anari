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
//#ifdef VISRTX_USE_NEURAL
//    else if (subtype == "neural")
//        return new Neural(d);
//#endif
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

CYCLES_ANARI_TYPEFOR_DEFINITION(anari_cycles::Geometry *);
