// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Light.h"
#include "Surface.h"
#include "Volume.h"

namespace anari_cycles {

struct Group : public Object
{
  Group(CyclesGlobalState *s);
  ~Group() override;

  void commitParameters() override;

  void addGroupToCurrentCyclesScene(const math::mat4 &xfm) const;

  box3 bounds() const override;
  
  // Accessor for light data (needed for HDRI light discovery)
  const ObjectArray* lightData() const { return m_lightData.get(); }


  bool containsTriangleGeometry() const;
  bool containsCurveGeometry() const;
  bool containsUserGeometry() const;
  bool containsVolumes() const;
  bool containsLights() const;

 private:
#if 0
  helium::ChangeObserverPtr<ObjectArray> m_surfaceData;
  helium::ChangeObserverPtr<ObjectArray> m_volumeData;
  helium::ChangeObserverPtr<ObjectArray> m_lightData;
#endif

  // Geometry //

  helium::ChangeObserverPtr<ObjectArray> m_surfaceData;

  std::vector<Surface*> m_surfacesTriangle;
  std::vector<Surface*> m_surfacesCurve;
  std::vector<Surface*> m_surfacesUser;

  //DeviceBuffer m_surfaceTriangleObjectIndices;
  //DeviceBuffer m_surfaceCurveObjectIndices;
  //DeviceBuffer m_surfaceUserObjectIndices;

  // Volume //

  helium::ChangeObserverPtr<ObjectArray> m_volumeData;
  std::vector<Volume*> m_volumes;

  //DeviceBuffer m_volumeObjectIndices;

  // Light //

  helium::ChangeObserverPtr<ObjectArray> m_lightData;
  std::vector<Light*> m_lights;

  //DeviceBuffer m_lightObjectIndices;
  //DeviceObjectIndex m_firstHDRI{ -1 };
};

} // namespace anari_cycles

CYCLES_ANARI_TYPEFOR_SPECIALIZATION(anari_cycles::Group *, ANARI_GROUP);
