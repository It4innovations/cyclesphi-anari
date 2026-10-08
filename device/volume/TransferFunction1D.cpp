// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "TransferFunction1D.h"
#include "XmlScene.h"
// std
#include <algorithm>
#include <vector>
// cycles
#include "scene/mesh.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"

namespace anari_cycles {

namespace {

// Piecewise-linear resample of a transfer function array to 'size' entries.
template <typename T>
T sampleArray(const std::vector<T> &values, float x)
{
  if (values.size() == 1)
    return values[0];
  const float f = ccl::clamp(x, 0.f, 1.f) * float(values.size() - 1);
  const size_t i0 = size_t(f);
  const size_t i1 = std::min(i0 + 1, values.size() - 1);
  const float t = f - float(i0);
  return values[i0] * (1.f - t) + values[i1] * t;
}

// cyclesphi: replace the built-in graph by the XML template
// transferFunction1D.xml when XML templates are enabled (see XmlScene.h). The
// template nodes are driven by the field, value range, unit distance and the
// color/opacity lookup table of the built-in graph.
void applyXmlTemplate(CyclesGlobalState &state,
    ccl::Shader *shader,
    SpatialField *field,
    const helium::box1 &valueRange,
    float unitDistance)
{
  if (!state.xmlTemplates || !field || !shader->graph)
    return;

  ccl::array<ccl::packed_float3> lut;
  ccl::array<float> lutAlpha;
  bool hasLut = false;
  for (ccl::ShaderNode *node : shader->graph->nodes) {
    if (node->type == ccl::RGBRampNode::get_node_type()) {
      auto *ramp = static_cast<ccl::RGBRampNode *>(node);
      lut = ramp->get_ramp();
      lutAlpha = ramp->get_ramp_alpha();
      hasLut = true;
    }
  }
  if (!hasLut || !xmlApplyShaderTemplate(state, shader, "transferFunction1D"))
    return;

  ccl::ShaderGraph *graph = shader->graph.get();
  ccl::MapRangeNode *mapRange = nullptr;
  ccl::RGBRampNode *ramp = nullptr;
  ccl::MathNode *math = nullptr;
  for (ccl::ShaderNode *node : graph->nodes) {
    if (node->type == ccl::MapRangeNode::get_node_type())
      mapRange = static_cast<ccl::MapRangeNode *>(node);
    else if (node->type == ccl::RGBRampNode::get_node_type())
      ramp = static_cast<ccl::RGBRampNode *>(node);
    else if (node->type == ccl::MathNode::get_node_type())
      math = static_cast<ccl::MathNode *>(node);
  }

  if (mapRange) {
    mapRange->set_from_min(valueRange.lower);
    mapRange->set_from_max(valueRange.upper);
    if (auto *fieldValue = field->createCyclesSamplingNodes(graph)) {
      graph->disconnect(mapRange->input("Value"));
      graph->connect(fieldValue, mapRange->input("Value"));
    }
  }
  if (ramp) {
    ramp->set_interpolate(true);
    ramp->set_ramp(lut);
    ramp->set_ramp_alpha(lutAlpha);
  }
  if (math)
    math->set_value2(unitDistance > 0.f ? unitDistance : 1.f);
}

} // namespace

TransferFunction1D::TransferFunction1D(CyclesGlobalState *s)
    : FieldVolume(s, "ANARI TransferFunction1D"),
      m_field(this),
      m_colorData(this),
      m_opacityData(this)
{}

TransferFunction1D::~TransferFunction1D() = default;

bool TransferFunction1D::isValid() const
{
  return m_field && m_field->isValid();
}

void TransferFunction1D::commitParameters()
{
  m_field = getParamObject<SpatialField>("value");
  m_valueRange = getParam<helium::box1>("valueRange", helium::box1{0.f, 1.f});
  m_colorData = getParamObject<Array1D>("color");
  m_uniformColor = {1.f, 1.f, 1.f, 1.f};
  getParam("color", ANARI_FLOAT32_VEC3, &m_uniformColor);
  getParam("color", ANARI_FLOAT32_VEC4, &m_uniformColor);
  m_opacityData = getParamObject<Array1D>("opacity");
  m_uniformOpacity = getParam<float>("opacity", 1.f);
  m_unitDistance = getParam<float>("unitDistance", 1.f);
  m_id = getParam<uint32_t>("id", ~0u);

  if (!m_field) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "no spatial field provided to transferFunction1D volume");
  }
}

void TransferFunction1D::finalize()
{
  if (!isValid()) {
    retireMesh();
    Volume::finalize();
    return;
  }

  m_bounds = m_field->bounds();

  rebuildCyclesShaderGraph();
  syncCyclesMesh({m_field.get()});

  Volume::finalize();
}

void TransferFunction1D::rebuildCyclesShaderGraph()
{
  auto &state = *deviceState();

  auto graph = std::make_unique<ccl::ShaderGraph>();

  auto *fieldValue = m_field->createCyclesSamplingNodes(graph.get());
  if (fieldValue) {
    auto *mapRange = graph->create_node<ccl::MapRangeNode>();
    mapRange->set_clamp(true);
    mapRange->set_from_min(m_valueRange.lower);
    mapRange->set_from_max(m_valueRange.upper);
    graph->connect(fieldValue, mapRange->input("Value"));

    // The color array may be float4 with opacity baked into alpha, float3
    // (implicit alpha of 1), or absent entirely (uniform 'color' parameter).
    // Color alpha and the opacity array/parameter multiply together.
    std::vector<ccl::float3> colors;
    std::vector<float> colorAlphas;
    if (!m_colorData) {
      colors.push_back(ccl::make_float3(
          m_uniformColor[0], m_uniformColor[1], m_uniformColor[2]));
      colorAlphas.push_back(m_uniformColor[3]);
    } else if (m_colorData->elementType() == ANARI_FLOAT32_VEC3) {
      auto *c = m_colorData->beginAs<anari_vec::float3>();
      for (size_t i = 0; i < m_colorData->size(); ++i)
        colors.push_back(ccl::make_float3(c[i][0], c[i][1], c[i][2]));
      colorAlphas.push_back(1.f);
    } else if (m_colorData->elementType() == ANARI_FLOAT32_VEC4) {
      auto *c = m_colorData->beginAs<anari_vec::float4>();
      for (size_t i = 0; i < m_colorData->size(); ++i) {
        colors.push_back(ccl::make_float3(c[i][0], c[i][1], c[i][2]));
        colorAlphas.push_back(c[i][3]);
      }
    } else {
      reportMessage(ANARI_SEVERITY_WARNING,
          "unsupported color array element type on transferFunction1D volume");
      colors.push_back(ccl::make_float3(1.f, 1.f, 1.f));
      colorAlphas.push_back(1.f);
    }

    std::vector<float> opacities;
    if (!m_opacityData) {
      opacities.push_back(m_uniformOpacity);
    } else if (m_opacityData->elementType() == ANARI_FLOAT32) {
      auto *o = m_opacityData->beginAs<float>();
      opacities.assign(o, o + m_opacityData->size());
    } else {
      reportMessage(ANARI_SEVERITY_WARNING,
          "unsupported opacity array element type on transferFunction1D volume");
      opacities.push_back(1.f);
    }

    // Guard against zero-length arrays (sampleArray would read out of bounds)
    if (colors.empty()) {
      colors.push_back(ccl::make_float3(1.f, 1.f, 1.f));
      colorAlphas.push_back(1.f);
    }
    if (opacities.empty())
      opacities.push_back(1.f);

    // Resample color/opacity onto a single shared LUT: Cycles' RGBRampNode
    // silently compiles to nothing when ramp/alpha sizes differ, and the ANARI
    // arrays may have different lengths. Size the LUT to preserve the finer of
    // the two inputs (the ramp itself interpolates between entries).
    const int lutSize = int(std::min<size_t>(
        std::max({colors.size(), opacities.size(), size_t(2)}), 4096));

    auto *ramp = graph->create_node<ccl::RGBRampNode>();
    ramp->set_interpolate(true);
    ramp->get_ramp().resize(lutSize);
    ramp->get_ramp_alpha().resize(lutSize);
    for (int i = 0; i < lutSize; ++i) {
      const float x = float(i) / float(lutSize - 1);
      ramp->get_ramp()[i] = sampleArray(colors, x);
      ramp->get_ramp_alpha()[i] =
          sampleArray(colorAlphas, x) * sampleArray(opacities, x);
    }
    graph->connect(mapRange->output("Result"), ramp->input("Fac"));

    // Per the spec, 'opacity' is the fraction of light absorbed over one
    // unitDistance: sigma_t = -ln(1 - opacity) / unitDistance. The opacity is
    // clamped below 1 to keep the extinction finite (an opacity of 1 then
    // absorbs all but 1e-5 of the light per unitDistance).
    auto *clamped = graph->create_node<ccl::MathNode>();
    clamped->set_math_type(ccl::NODE_MATH_MINIMUM);
    graph->connect(ramp->output("Alpha"), clamped->input("Value1"));
    clamped->set_value2(1.f - 1e-5f);

    auto *transmittance = graph->create_node<ccl::MathNode>();
    transmittance->set_math_type(ccl::NODE_MATH_SUBTRACT);
    transmittance->set_value1(1.f);
    graph->connect(clamped->output("Value"), transmittance->input("Value2"));

    auto *logT = graph->create_node<ccl::MathNode>();
    logT->set_math_type(ccl::NODE_MATH_LOGARITHM);
    graph->connect(transmittance->output("Value"), logT->input("Value1"));
    logT->set_value2(2.718281828459045f); // natural logarithm

    auto *density = graph->create_node<ccl::MathNode>();
    density->set_math_type(ccl::NODE_MATH_MULTIPLY);
    density->set_value2(-1.f / (m_unitDistance > 0.f ? m_unitDistance : 1.f));
    graph->connect(logT->output("Value"), density->input("Value1"));
    ccl::ShaderOutput *sigma = density->output("Value");

    // Fields that do not fill their bounds (e.g. unstructured meshes) scale
    // the extinction by the coverage of their domain.
    if (auto *coverage = m_field->createCyclesCoverageNodes(graph.get())) {
      auto *masked = graph->create_node<ccl::MathNode>();
      masked->set_math_type(ccl::NODE_MATH_MULTIPLY);
      graph->connect(sigma, masked->input("Value1"));
      graph->connect(coverage, masked->input("Value2"));
      sigma = masked->output("Value");
    }

    auto *volumeNode = graph->create_node<ccl::PrincipledVolumeNode>();
    volumeNode->set_density_attribute(ustring());
    volumeNode->set_color_attribute(ustring());
    volumeNode->set_temperature_attribute(ustring());
    volumeNode->set_blackbody_intensity(0.f);
    // Emission + absorption (no scattering albedo): this matches the alpha
    // compositing model reference devices use for transferFunction1D, where
    // sample color is accumulated proportional to opacity.
    volumeNode->set_color(ccl::zero_float3());
    graph->connect(sigma, volumeNode->input("Density"));
    graph->connect(ramp->output("Color"), volumeNode->input("Emission Color"));
    graph->connect(sigma, volumeNode->input("Emission Strength"));

    graph->connect(
        volumeNode->output("Volume"), graph->output()->input("Volume"));
  } else {
    reportMessage(ANARI_SEVERITY_WARNING,
        "transferFunction1D volume could not create field sampling nodes");
  }

  m_shader->set_graph(std::move(graph));
  applyXmlTemplate(state, m_shader, m_field.get(), m_valueRange, m_unitDistance);
  applyVolumeStepRate(m_field.get());
  m_shader->tag_update(state.scene);
}

} // namespace anari_cycles
