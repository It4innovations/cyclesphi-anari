// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "XmlScene.h"
// cycles
#include "app/cycles_xml_bin.h"
#include "scene/background.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "util/path.h"
// std
#include <cstdlib>
#include <cstring>

namespace anari_cycles {

static bool envFlag(const char *name)
{
  const char *value = std::getenv(name);
  return value && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

void xmlConfigure(CyclesGlobalState &state,
    const std::string &xmlPathParam,
    bool xmlSceneParam,
    bool xmlTemplatesParam)
{
  const char *envPath = std::getenv("CYCLES_XML_PATH");

  if (!xmlPathParam.empty())
    state.xmlPath = xmlPathParam;
  else if (envPath && envPath[0] != '\0')
    state.xmlPath = envPath;
  else if (xmlSceneParam || xmlTemplatesParam || envFlag("CYCLES_ANARI_XML_SCENE")
      || envFlag("CYCLES_ANARI_XML_TEMPLATES"))
    state.xmlPath = ccl::path_get("anari");
  else
    state.xmlPath.clear();

  state.xmlTemplates = !state.xmlPath.empty()
      && (xmlTemplatesParam || envFlag("CYCLES_ANARI_XML_TEMPLATES"));
}

static ccl::BackgroundNode *findBackgroundNode(
    ccl::ShaderGraph *graph, const char *name)
{
  if (!graph)
    return nullptr;
  for (ccl::ShaderNode *node : graph->nodes) {
    if (node->type == ccl::BackgroundNode::get_node_type()
        && node->name == ccl::ustring(name))
      return static_cast<ccl::BackgroundNode *>(node);
  }
  return nullptr;
}

void xmlLoadDefaultScene(CyclesGlobalState &state)
{
  state.xmlBgColor = nullptr;
  state.xmlAmbientIntensity = nullptr;

  if (state.xmlPath.empty())
    return;

  const std::string filepath =
      ccl::path_join(state.xmlPath, "cycles_default_scene.xml");
  if (!ccl::path_exists(filepath))
    return;

  ccl::Scene *scene = state.scene;
  const ccl::BVHType bvhType = scene->params.bvh_type;
  ccl::xml_read_file(scene, filepath.c_str());
  // The XML reader forces a static BVH, keep the session setting.
  scene->params.bvh_type = bvhType;

  // Objects of the XML scene are replaced by the ANARI world on the first
  // world sync, only the film, integrator and shader settings are kept.
  ccl::Shader *background = scene->default_background;
  if (background && background->graph) {
    state.xmlBgColor = findBackgroundNode(background->graph.get(), "bgColor");
    state.xmlAmbientIntensity =
        findBackgroundNode(background->graph.get(), "ambientIntensity");
  }
}

bool xmlApplyShaderTemplate(
    CyclesGlobalState &state, ccl::Shader *shader, const std::string &name)
{
  if (!state.xmlTemplates || !shader)
    return false;

  const std::string filepath = ccl::path_join(state.xmlPath, name + ".xml");
  if (!ccl::path_exists(filepath))
    return false;

  ccl::xml_set_material_to_shader2(state.scene, shader, filepath.c_str());
  return shader->graph != nullptr;
}

} // namespace anari_cycles
