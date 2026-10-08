// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Sampler.h"
// std
#include <string>

namespace anari_cycles {

struct UnknownSampler : public Sampler
{
  UnknownSampler(std::string_view subtype, CyclesGlobalState *s)
      : Sampler(s), m_subtype(subtype)
  {
    reportMessage(ANARI_SEVERITY_WARNING,
        "created unknown ANARI_SAMPLER object of subtype '%s'",
        m_subtype.c_str());
  }

  bool isValid() const override
  {
    return false;
  }

  void warnIfUnknownObject() const override
  {
    reportMessage(ANARI_SEVERITY_WARNING,
        "encountered unknown ANARI_SAMPLER object of subtype '%s'",
        m_subtype.c_str());
  }

 private:
  std::string m_subtype;
};

} // namespace anari_cycles
