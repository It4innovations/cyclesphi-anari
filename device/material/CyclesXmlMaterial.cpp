// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "CyclesXmlMaterial.h"
// cycles
#include "scene/scene.h"

namespace anari_cycles {

CyclesXmlMaterial::CyclesXmlMaterial(CyclesGlobalState *s)
    : Material(s), m_xml(this, "'cyclesXml' material: 'source'")
{}

void CyclesXmlMaterial::commitParameters()
{
  m_source = getParamString("source", "");
}

void CyclesXmlMaterial::finalize()
{
  // finalize() also runs when an observed sampler was committed, or when a
  // parameter the graph does not depend on changed: keep the graph (and the
  // compiled shader) unless the text or an image it uses is different.
  if (!m_xml.changed(m_source)) {
    Object::finalize();
    return;
  }

  buildGraph();
  Material::finalize();
}

// The geometries have one tangent: the standard Cycles UV tangent (+ sign),
// written from 'vertex.tangent' (see Mesh::setTangents()). They have no
// tangents per UV map and no undisplaced copies (what a normal map asks for
// with its default 'original' base); Cycles would generate those itself, from
// UVs the mesh does not have. Make the nodes use the tangent that is there,
// like the normal maps of the 'physicallyBased' material.
static void useGeometryTangent(ccl::NormalMapNode *node)
{
  if (node->get_space() != ccl::NODE_NORMAL_MAP_TANGENT)
    return;
  node->set_base(ccl::NODE_NORMAL_MAP_BASE_DISPLACED);
  node->set_attribute(ccl::ustring());
}

static void useGeometryTangent(ccl::TangentNode *node)
{
  if (node->get_direction_type() == ccl::NODE_TANGENT_UVMAP)
    node->set_attribute(ccl::ustring());
}

void CyclesXmlMaterial::buildGraph()
{
  auto &state = *deviceState();

  if (!m_shader)
    m_shader = state.scene->create_node<ccl::Shader>();

  // The <shader> sockets are read into a temporary: sockets the text does not
  // mention get their default value again, and only values which really
  // change are tagged as modified on the scene shader.
  auto settings = std::make_unique<ccl::Shader>();
  std::unique_ptr<ccl::ShaderGraph> graph =
      m_xml.build(m_source, *settings, false, "rendering a gray fallback");

  if (graph) {
    for (ccl::ShaderNode *node : graph->nodes) {
      if (node->type == ccl::NormalMapNode::get_node_type())
        useGeometryTangent(static_cast<ccl::NormalMapNode *>(node));
      else if (node->type == ccl::TangentNode::get_node_type())
        useGeometryTangent(static_cast<ccl::TangentNode *>(node));
    }
  } else {
    settings = std::make_unique<ccl::Shader>();
    graph = std::make_unique<ccl::ShaderGraph>();
    auto *diffuse = graph->create_node<ccl::DiffuseBsdfNode>();
    diffuse->set_color(make_float3(0.8f, 0.8f, 0.8f));
    graph->connect(diffuse->output("BSDF"), graph->output()->input("Surface"));
  }

  XmlShaderGraph::applySettings(*m_shader, *settings);

  // The id frame channels need their AOV nodes in every surface graph.
  addIdChannelNodes(graph.get());

  reportMessage(ANARI_SEVERITY_DEBUG,
      "'cyclesXml' material: built shader graph (%d nodes, %d image names)",
      int(graph->nodes.size()),
      int(m_xml.numImages()));

  m_graphOwned = std::move(graph);
  m_graph = m_graphOwned.get();
}

} // namespace anari_cycles
