// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Array.h"
#include "Object.h"
// cycles
#include "scene/geometry.h"
// std
#include <optional>

namespace anari_cycles {

struct UniformAttributes
{
    helium::IntrusivePtr<Array1D> attribute0Array;
    helium::IntrusivePtr<Array1D> attribute1Array;
    helium::IntrusivePtr<Array1D> attribute2Array;
    helium::IntrusivePtr<Array1D> attribute3Array;
    helium::IntrusivePtr<Array1D> colorArray;

    std::optional<anari_vec::vec4> attribute0;
    std::optional<anari_vec::vec4> attribute1;
    std::optional<anari_vec::vec4> attribute2;
    std::optional<anari_vec::vec4> attribute3;
    std::optional<anari_vec::vec4> color;
};

struct GeometryAttributes
{
    helium::IntrusivePtr<Array1D> attribute0;
    helium::IntrusivePtr<Array1D> attribute1;
    helium::IntrusivePtr<Array1D> attribute2;
    helium::IntrusivePtr<Array1D> attribute3;
    helium::IntrusivePtr<Array1D> color;
};


struct Geometry : public Object
{
  Geometry(CyclesGlobalState *s);
  ~Geometry() override;

  static Geometry *createInstance(
      std::string_view type, CyclesGlobalState *state);

  virtual void finalize() override;

  void commitParameters() override;
  void markFinalized() override;

  virtual ccl::Geometry *createCyclesGeometryNode() = 0;
  virtual void syncCyclesNode(ccl::Geometry *node) const = 0;

  void commitAttributes(const char* prefix, GeometryAttributes& attrs);
  //void populateAttributeDataSet(
  //    const GeometryAttributes& hostAttrs, AttributeDataSet& gpuAttrs) const;

  GeometryAttributes m_primitiveAttributes;
  UniformAttributes m_uniformAttributes;
  helium::IntrusivePtr<Array1D> m_primitiveId;
};

} // namespace anari_cycles

CYCLES_ANARI_TYPEFOR_SPECIALIZATION(anari_cycles::Geometry *, ANARI_GEOMETRY);
