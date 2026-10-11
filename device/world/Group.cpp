// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "Group.h"
// cycles
#include "kernel/types.h"
#include "scene/object.h"
// std
#include <algorithm>

namespace anari_cycles {

Group::Group(CyclesGlobalState *s)
    : Object(ANARI_GROUP, s),
      m_surfaceData(this),
      m_volumeData(this),
      m_lightData(this)
{}

Group::~Group() = default;

void Group::commitParameters()
{
  m_surfaceData = getParamObject<ObjectArray>("surface");
  m_volumeData = getParamObject<ObjectArray>("volume");
  m_lightData = getParamObject<ObjectArray>("light");
}

bool Group::addGroupToCurrentCyclesScene(const math::mat4 &xfm,
    const helium::box1 &shutter,
    const std::vector<ccl::Transform> *motion,
    uint32_t instanceId,
    uint32_t instanceVisibility,
    const ObjectInfo *objectInfo) const
{
  auto &state = *deviceState();
  bool deformationMotion = false;

  auto cxfm = mat4ToCycles(xfm);

  // Object::set_motion() steals the array, so every object needs its own
  // copy of the baked motion steps.
  auto setMotion = [&](ccl::Object *o) {
    if (!motion || motion->size() < 2)
      return;
    ccl::array<ccl::Transform> steps;
    steps.resize(motion->size());
    std::copy(motion->begin(), motion->end(), steps.data());
    o->set_motion(steps);
  };

  // The instance 'id' feeds the 'instanceId' frame channel through a
  // per-object float attribute read by the OutputAOV nodes in every surface
  // material (see Material::makeGraph()). Stored biased by +1 -- exact in a
  // float for ids < 2^24 -- so the "no id" default (~0u) becomes 0, the
  // value untouched AOV pixels read back anyway. Volumes get the attribute
  // too, but Cycles never runs AOV nodes in volume shading, so volume
  // pixels always read back ~0u.
  const float instanceIdAttr = instanceId == ~0u ? 0.f : instanceId + 1.f;
  auto setInstanceId = [&](ccl::Object *o) {
    o->attributes.emplace_back(
        OIIO::ustring("instanceId"), OIIO::TypeFloat, 1, &instanceIdAttr);
  };

  if (m_surfaceData) {
    auto **surfacesBegin = (Surface **)m_surfaceData->handlesBegin();
    auto **surfacesEnd = (Surface **)m_surfaceData->handlesEnd();

    std::for_each(surfacesBegin, surfacesEnd, [&](Surface *s) {
      if (!s->isValid()) {
        s->warnIfUnknownObject();
        return;
      }
      if (!s->cyclesGeometry())
        return;
      // Like Blender's Cycles: no object for something no ray can see.
      const uint32_t visibility = s->visibilityMask() & instanceVisibility;
      if ((visibility & ccl::PATH_RAY_VISIBILITY_ALL) == 0)
        return;
      // KHR_GEOMETRY_*_MOTION_DEFORMATION: (re)bake the geometry's motion
      // keys onto this shutter (cached; no-op for non-deforming geometry).
      deformationMotion |= s->bakeGeometryMotion(shutter);
      auto *o = state.scene->create_node<ccl::Object>();
      o->set_geometry(s->cyclesGeometry());
      o->set_tfm(cxfm);
      setMotion(o);
      setInstanceId(o);
      o->set_pass_id(s->id());
      // Object Info shader node: the kernel's random number of an object is
      // random_id / 0xFFFFFFFF.
      if (objectInfo && objectInfo->hasRandom) {
        const double random =
            std::clamp(double(objectInfo->random), 0.0, 1.0) * 4294967295.0;
        o->set_random_id(uint32_t(random + 0.5));
      }
      if (objectInfo && objectInfo->hasColor) {
        o->set_color(ccl::make_float3(objectInfo->color[0],
            objectInfo->color[1],
            objectInfo->color[2]));
        o->set_alpha(objectInfo->color[3]);
      }
      // Ray visibility: the flags of the ANARI Surface
      // (CYCLES_SURFACE_COMPOSITING, shared by every instance of the
      // surface) and the ones of the instance (CYCLES_INSTANCE_VISIBILITY).
      // set_visibility() no-ops at the default mask (~0u). The compositing
      // flags live on the surface only.
      o->set_visibility(visibility);
      o->set_use_holdout(s->holdout());
      o->set_is_shadow_catcher(s->shadowCatcher());
      // CYCLES_LIGHT_LINKING receiver/blocker set indices and the
      // CYCLES_LIGHTGROUPS emission routing; all setters no-op at their
      // defaults (0 / 0 / empty).
      o->set_receiver_light_set(s->receiverLightSet());
      o->set_blocker_shadow_set(s->blockerShadowSet());
      o->set_lightgroup(OIIO::ustring(s->lightGroup()));
    });
  }

  if (m_volumeData) {
    auto **volumesBegin = (Volume **)m_volumeData->handlesBegin();
    auto **volumesEnd = (Volume **)m_volumeData->handlesEnd();

    std::for_each(volumesBegin, volumesEnd, [&](Volume *v) {
      if (!v->isValid()) {
        v->warnIfUnknownObject();
        return;
      }
      if (!v->cyclesGeometry())
        return;
      if ((instanceVisibility & ccl::PATH_RAY_VISIBILITY_ALL) == 0)
        return;
      auto *o = state.scene->create_node<ccl::Object>();
      o->set_geometry(v->cyclesGeometry());
      o->set_tfm(cxfm);
      setMotion(o);
      setInstanceId(o);
      o->set_pass_id(v->id());
      o->set_visibility(instanceVisibility);
    });
  }

  if (m_lightData) {
    auto **lightsBegin = (Light **)m_lightData->handlesBegin();
    auto **lightsEnd = (Light **)m_lightData->handlesEnd();

    if (motion && motion->size() > 1 && lightsBegin != lightsEnd) {
      reportMessage(ANARI_SEVERITY_WARNING,
          "lights in a motion instance do not motion blur (unsupported by "
          "Cycles) -- placing them at the shutter-start pose");
    }

    std::for_each(lightsBegin, lightsEnd, [&](Light *l) {
      if (!l->isValid()) {
        l->warnIfUnknownObject();
        return;
      }
      auto makeLightObject = [&](ccl::Light *cl, const math::mat4 &lightXfm) {
        auto *o = state.scene->create_node<ccl::Object>();
        o->set_geometry(cl);
        o->set_tfm(mat4ToCycles(math::mul(xfm, lightXfm)));
        // On lights this flag only means "illuminates the shadow-catcher
        // sub-path" (the unshadowed reference a 'shadowCatcher' surface is
        // divided by). Blender sets it on every light by default; without it
        // the sub-path sees no light and catchers record no shadows.
        o->set_is_shadow_catcher(true);
        // KHR_AREA_LIGHTS 'visible': hide the light geometry from camera
        // rays (Cycles turns this into SHADER_EXCLUDE_CAMERA on the light);
        // illumination of the scene is unaffected.
        if (!l->visibleToCamera())
          o->set_visibility(o->get_visibility() & ~ccl::PATH_RAY_VISIBILITY_CAMERA);
        // CYCLES_INSTANCE_VISIBILITY: for a light the flags say which kinds
        // of rays it lights (Cycles excludes it from the others).
        if (instanceVisibility != ~0u)
          o->set_visibility(o->get_visibility() & instanceVisibility);
        // The same flags of the light itself ('visibility.*').
        if (l->rayVisibility() != ~0u)
          o->set_visibility(o->get_visibility() & l->rayVisibility());
        // CYCLES_LIGHT_LINKING: which receiver sets this light illuminates
        // and which blocker sets shadow it (default ~0 = all sets), plus the
        // CYCLES_LIGHTGROUPS pass its emission accumulates into. Setters
        // no-op at the defaults.
        o->set_light_set_membership(l->lightSetMembership());
        o->set_shadow_set_membership(l->shadowSetMembership());
        o->set_lightgroup(OIIO::ustring(l->lightGroup()));
      };
      makeLightObject(l->cyclesLight(), l->xfm());
      // Second emitter for e.g. two-sided quad lights.
      if (auto *second = l->secondaryCyclesLight())
        makeLightObject(second, l->secondaryXfm());
    });
  }

  return deformationMotion;
}

bool Group::hasDeformingSurfaces() const
{
  if (!m_surfaceData)
    return false;
  auto **surfacesBegin = (Surface **)m_surfaceData->handlesBegin();
  auto **surfacesEnd = (Surface **)m_surfaceData->handlesEnd();
  // Only surfaces that can render count (invalid ones are skipped by
  // addGroupToCurrentCyclesScene(), so their keys cannot depend on the
  // shutter).
  return std::any_of(surfacesBegin, surfacesEnd, [](const Surface *s) {
    return s->isValid() && s->geometry()->hasDeformationMotion();
  });
}

box3 Group::bounds() const
{
  box3 b = empty_box3();
  if (m_surfaceData) {
    auto **surfacesBegin = (Surface **)m_surfaceData->handlesBegin();
    auto **surfacesEnd = (Surface **)m_surfaceData->handlesEnd();

    std::for_each(surfacesBegin, surfacesEnd, [&](Surface *s) {
      if (s->isValid())
        extend(b, s->geometry()->bounds());
      else
        s->warnIfUnknownObject();
    });
  }
  if (m_volumeData) {
    auto **volumesBegin = (Volume **)m_volumeData->handlesBegin();
    auto **volumesEnd = (Volume **)m_volumeData->handlesEnd();

    std::for_each(volumesBegin, volumesEnd, [&](Volume *s) {
      if (s->isValid())
        extend(b, s->bounds());
      else
        s->warnIfUnknownObject();
    });
  }
  return b;
}

} // namespace anari_cycles

CYCLES_ANARI_TYPEFOR_DEFINITION(anari_cycles::Group *);
