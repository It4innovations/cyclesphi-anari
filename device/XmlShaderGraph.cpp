// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "XmlShaderGraph.h"
#include "sampler/Image2D.h"
#include "sampler/SamplerUtils.h"
// cycles
#include "app/cycles_xml_bin.h"
#include "scene/image.h"
#include "scene/scene.h"
#include "scene/shader_nodes.h"

namespace anari_cycles {

// Reader messages are collected into one status message per severity, at
// most this many are spelled out.
constexpr int MAX_REPORTED_MESSAGES = 8;

struct XmlMessages
{
  void add(const std::string &message)
  {
    if (count++ < MAX_REPORTED_MESSAGES)
      text += (text.empty() ? "" : " ") + message;
  }

  std::string summary() const
  {
    if (count <= MAX_REPORTED_MESSAGES)
      return text;
    return text + " (and " + std::to_string(count - MAX_REPORTED_MESSAGES)
        + " more)";
  }

  std::string text;
  int count{0};
};

XmlShaderGraph::XmlShaderGraph(Object *owner, std::string what)
    : m_owner(owner), m_what(std::move(what))
{}

bool XmlShaderGraph::changed(const std::string &source)
{
  return !m_built || source != m_builtSource || imagesChanged();
}

bool XmlShaderGraph::valid() const
{
  return m_built && m_valid;
}

void XmlShaderGraph::clear()
{
  m_images.clear();
  m_builtSource.clear();
  m_built = false;
  m_valid = false;
}

size_t XmlShaderGraph::numImages() const
{
  return m_images.size();
}

void XmlShaderGraph::applySettings(
    ccl::Shader &shader, const ccl::Shader &settings)
{
  for (const ccl::SocketType &socket : shader.type->inputs)
    shader.set_value(socket, settings, socket);
}

Array2D *XmlShaderGraph::imageArray(
    const std::string &name, Sampler **sampler) const
{
  *sampler = m_owner->getParamObject<Sampler>(name);
  auto *image2D = dynamic_cast<Image2D *>(*sampler);
  return image2D ? image2D->image() : nullptr;
}

bool XmlShaderGraph::imagesChanged()
{
  bool changed = false;
  for (ImageBinding &binding : m_images) {
    Sampler *sampler = nullptr;
    Array2D *array = imageArray(binding.name, &sampler);
    // Another sampler with the same array needs no new graph, only observing.
    if (sampler != binding.sampler.get())
      binding.sampler = sampler;
    changed |= array != binding.array.get()
        || (array && array->lastDataModified() != binding.dataStamp);
  }
  return changed;
}

XmlShaderGraph::ImageBinding &XmlShaderGraph::imageBinding(
    const std::string &name)
{
  for (ImageBinding &binding : m_images) {
    if (binding.name == name)
      return binding;
  }

  ImageBinding &binding = m_images.emplace_back(name, m_owner);
  Sampler *sampler = nullptr;
  binding.array = imageArray(name, &sampler);
  binding.sampler = sampler;
  if (binding.array)
    binding.dataStamp = binding.array->lastDataModified();
  else {
    m_owner->reportMessage(ANARI_SEVERITY_WARNING,
        "%s: image file name '%s' is not the name of an 'image2D' sampler "
        "parameter with an image, the node is left as is",
        m_what.c_str(),
        name.c_str());
  }
  return binding;
}

template <typename NODE_T>
void XmlShaderGraph::bindImage(NODE_T *node)
{
  const std::string name = node->get_filename().string();
  if (name.empty() || !node->handle.empty())
    return;

  Array2D *array = imageBinding(name).array.get();
  if (!array)
    return;

  // Interpolation, extension and alpha type are the ones of the node. The
  // color space follows the element type of the array, like in the image2D
  // sampler (the loader decodes the sRGB types, everything else is linear).
  ccl::ImageParams params = node->image_params();
  params.animated = false;
  params.colorspace = imageColorspace(array->elementType());
  node->handle = m_owner->deviceState()->scene->image_manager->add_image(
      std::make_unique<SamplerImageLoader>(array), params, false);
  // Cycles must never look for a file of that name.
  node->set_filename(ccl::ustring());
}

std::unique_ptr<ccl::ShaderGraph> XmlShaderGraph::build(
    const std::string &source,
    ccl::Shader &settings,
    bool surfaceOnly,
    const char *fallback)
{
  m_images.clear();
  m_builtSource = source;
  m_built = true;
  m_valid = false;

  XmlMessages warnings;
  XmlMessages errors;
  auto report = [&](bool error, const std::string &message) {
    (error ? errors : warnings).add(message);
  };

  std::unique_ptr<ccl::ShaderGraph> graph =
      ccl::xml_read_shader_graph_from_string(m_owner->deviceState()->scene,
          &settings,
          source.data(),
          source.size(),
          report);

  if (graph && errors.count == 0) {
    ccl::OutputNode *output = graph->output();
    if (surfaceOnly) {
      if (!output->input("Surface")->link)
        errors.add("Nothing is connected to the surface of the output node.");
    } else if (!output->input("Surface")->link
        && !output->input("Volume")->link
        && !output->input("Displacement")->link)
      errors.add("Nothing is connected to the output node.");
  }

  if (!graph || errors.count > 0) {
    m_owner->reportMessage(ANARI_SEVERITY_ERROR,
        "%s cannot be used, %s -- %s",
        m_what.c_str(),
        fallback,
        errors.summary().c_str());
    return nullptr;
  }

  for (ccl::ShaderNode *node : graph->nodes) {
    if (node->type == ccl::ImageTextureNode::get_node_type())
      bindImage(static_cast<ccl::ImageTextureNode *>(node));
    else if (node->type == ccl::EnvironmentTextureNode::get_node_type())
      bindImage(static_cast<ccl::EnvironmentTextureNode *>(node));
  }

  if (warnings.count > 0) {
    m_owner->reportMessage(ANARI_SEVERITY_WARNING,
        "%s: %d problem(s) in the text -- %s",
        m_what.c_str(),
        warnings.count,
        warnings.summary().c_str());
  }

  m_valid = true;
  return graph;
}

} // namespace anari_cycles
