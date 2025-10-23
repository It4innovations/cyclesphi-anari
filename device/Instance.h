// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Group.h"

namespace anari_cycles {

struct Instance : public Object
{
  Instance(CyclesGlobalState *s);
  ~Instance() override;

  void commitParameters() override;

  Group *group() const;

  void addInstanceObjectsToCyclesScene();

  box3 bounds() const override;

  bool isValid() const override;

 private:
#if 0
  helium::IntrusivePtr<Group> m_group;
  helium::ChangeObserverPtr<Array1D> m_xfmArray;
  math::mat4 m_xfm;
#endif

  helium::ChangeObserverPtr<Array1D> m_xfmArray;
  math::mat4 m_xfm;
  helium::IntrusivePtr<Group> m_group;
  //UniformAttributes m_uniformAttributes;
  helium::ChangeObserverPtr<Array1D> m_idArray;
  uint32_t m_id{ ~0u };
};

} // namespace anari_cycles

CYCLES_ANARI_TYPEFOR_SPECIALIZATION(anari_cycles::Instance *, ANARI_INSTANCE);
