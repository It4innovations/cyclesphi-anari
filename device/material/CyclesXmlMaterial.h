// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Material.h"
#include "XmlShaderGraph.h"
// std
#include <string>

namespace anari_cycles {

// CyclesXmlMaterial //////////////////////////////////////////////////////////
//
// CYCLES_MATERIAL_CYCLES_XML: 'cyclesXml' material subtype taking a whole
// Cycles shader graph. 'source' (STRING) holds one <shader> element in the
// dialect of cyclesphi's XML reader (app/cycles_xml_bin.cpp): node elements
// named after the Cycles node type with a 'name' attribute and
// <socket name=".." value=".."/> children, followed by
// <connect from_node from_socket to_node to_socket/> elements; the output
// node is addressed as 'output'. <socket> children of the <shader> element
// itself set the shader settings (displacement_method, ...).
//
// Images are passed as 'image2D' sampler parameters of any name: an
// image_texture/environment_texture node whose 'filename' equals the name of
// such a parameter samples the array of that sampler. Only the array is
// used; filter and wrap modes come from the node (see XmlShaderGraph.h).
//
// Geometry attributes are read with attribute nodes named 'vertex.color' and
// 'vertex.attribute0'..'vertex.attribute7'. Tangent space normal maps (and
// UV map tangent nodes) always use the tangent of the geometry
// ('vertex.tangent'), whatever UV map and base they name.
//
// The graph is only parsed and handed to Cycles again when 'source' or one of
// the images it uses changed. A text that cannot be used (malformed XML,
// unknown node type, ...) reports an error and renders a gray diffuse
// fallback.

struct CyclesXmlMaterial : public Material
{
  CyclesXmlMaterial(CyclesGlobalState *s);
  ~CyclesXmlMaterial() override = default;

  void commitParameters() override;
  void finalize() override;

 private:
  void buildGraph();

  std::string m_source;
  XmlShaderGraph m_xml;
};

} // namespace anari_cycles
