// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "Spot.h"
#include "LightUtils.h"
// std
#include <algorithm>
// cycles
#include "kernel/svm/types.h"
#include "scene/background.h"
#include "scene/camera.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "util/math_base.h"
#include "util/transform.h"
#include "util/types_float3.h"
// std
#include <cmath>

namespace anari_cycles {

Spot::Spot(CyclesGlobalState *s)
    : Light(s, s->scene->create_node<ccl::SpotLight>())
{
  attachUnitEmissionShader();
}

void Spot::commitParameters()
{
  Light::commitParameters();
  m_position = getParam<math::float3>("position", {0.f, 0.f, 0.f});
  m_direction = getNormalizedDirection("direction", {0.f, 0.f, -1.f});
  // Photometric precedence: 'intensity' (W/sr) over 'power' (W).
  m_usesPower =
      !hasParam("intensity", ANARI_FLOAT32) && hasParam("power", ANARI_FLOAT32);
  m_value = std::clamp(m_usesPower ? getParam<float>("power", 1.f)
                                   : getParam<float>("intensity", 1.f),
      0.f,
      std::numeric_limits<float>::max());
  // ANARI 'openingAngle' is the full apex angle of the cone (default pi),
  // matching the Cycles spot 'angle' socket.
  m_openingAngle =
      std::clamp(getParam<float>("openingAngle", float(M_PI)), 0.f, float(M_PI));
  m_falloffAngle = getParam<float>("falloffAngle", 0.1f);
  m_radius = getParam<float>("radius", 0.f);
  m_softFalloff = getParam<bool>("softFalloff", false);
}

void Spot::finalize()
{
  auto *light = static_cast<ccl::SpotLight *>(m_cyclesLight);
  light->set_radius(m_radius);
  light->set_is_sphere(!m_softFalloff);
  light->set_angle(m_openingAngle);
  // ANARI 'falloffAngle' is the angular size of the region over which
  // intensity falls off toward the rim. Cycles 'smooth' is that region's
  // fraction of the cone measured in cosine space:
  // smooth = (cos(inner) - cos(half)) / (1 - cos(half)).
  const float halfAngle = 0.5f * m_openingAngle;
  const float innerAngle =
      std::clamp(halfAngle - m_falloffAngle, 0.f, halfAngle);
  const float cosHalf = std::cos(halfAngle);
  light->set_smooth(cosHalf < 1.f
          ? std::clamp(
                (std::cos(innerAngle) - cosHalf) / (1.f - cosHalf), 0.f, 1.f)
          : 0.f);
  light->set_normalize(true);
  // Same conversions as point lights (the cone only masks emission; neither
  // ANARI nor Cycles renormalizes flux into the cone): 'intensity' (W/sr)
  // maps to normalized strength 4*pi*intensity, 'power' (W) is the
  // normalized strength itself.
  m_cyclesLight->set_strength(
      scaledColor(m_usesPower ? m_value : 4.f * float(M_PI) * m_value));
  m_cyclesLight->tag_update(deviceState()->scene);
  Light::finalize();
}

math::mat4 Spot::xfm() const
{
  return positionDirectionXfm(m_position, m_direction);
}

} // namespace anari_cycles
