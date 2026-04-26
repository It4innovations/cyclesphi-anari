// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Instance.h"

namespace anari_cycles {

struct World : public Object
{
  World(CyclesGlobalState *s);
  ~World() override;

  void commitParameters() override;
  void finalize() override;

  void setCyclesWorldObjects();

  Light *findFirstHDRILight() const;

  box3 bounds() const override;

 private:
  void setupHDRIBackground();

#if 0
  helium::ChangeObserverPtr<ObjectArray> m_zeroSurfaceData;
  helium::ChangeObserverPtr<ObjectArray> m_zeroLightData;
  helium::ChangeObserverPtr<ObjectArray> m_zeroVolumeData;
  helium::IntrusivePtr<Group> m_zeroGroup;
  helium::IntrusivePtr<Instance> m_zeroInstance;

  helium::IntrusivePtr<ObjectArray> m_instanceData;
#endif

  helium::ChangeObserverPtr<ObjectArray> m_zeroSurfaceData;
  helium::ChangeObserverPtr<ObjectArray> m_zeroVolumeData;
  helium::ChangeObserverPtr<ObjectArray> m_zeroLightData;

  helium::ChangeObserverPtr<ObjectArray> m_instanceData;
  std::vector<Instance*> m_instances;

  helium::IntrusivePtr<Group> m_zeroGroup;
  helium::IntrusivePtr<Instance> m_zeroInstance;

  size_t m_numTriangleInstances{ 0 };
  size_t m_numCurveInstances{ 0 };
  size_t m_numUserInstances{ 0 };
  size_t m_numVolumeInstances{ 0 };
  size_t m_numLightInstances{ 0 };

};

} // namespace anari_cycles

CYCLES_ANARI_TYPEFOR_SPECIALIZATION(anari_cycles::World *, ANARI_WORLD);
