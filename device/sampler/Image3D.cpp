// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "Image3D.h"
#include "SamplerUtils.h"
#include "cycles_math.h"
// std
#include <algorithm>
#include <cmath>

namespace anari_cycles {

Image3D::Image3D(CyclesGlobalState *d) : Sampler(d), m_image(this) {}

bool Image3D::isValid() const
{
  return m_image && m_image->size(0) > 0 && m_image->size(1) > 0
      && m_image->size(2) > 0;
}

void Image3D::commitParameters()
{
  Sampler::commitParameters();
  m_image = getParamObject<Array3D>("image");
  m_linearFilter = getParamString("filter", "linear") != "nearest";
  m_wrapMode[0] =
      samplerWrapFromString(getParamString("wrapMode1", "clampToEdge"));
  m_wrapMode[1] =
      samplerWrapFromString(getParamString("wrapMode2", "clampToEdge"));
  m_wrapMode[2] =
      samplerWrapFromString(getParamString("wrapMode3", "clampToEdge"));
  m_borderColor = getParam<helium::float4>(
      "borderColor", helium::float4(0.f, 0.f, 0.f, 0.f));
}

void Image3D::finalize()
{
  m_handle = ccl::ImageHandle();

  if (m_image && !isValid()) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "'image' on 'image3D' sampler has a zero-sized dimension");
  } else if (!m_image) {
    reportMessage(ANARI_SEVERITY_WARNING,
        "missing required parameter 'image' on 'image3D' sampler");
  }

  if (isValid()) {
    for (int i = 0; i < 3; i++)
      m_dims[i] = uint32_t(m_image->size(i));

    // Tile the padded Z slices into a roughly square atlas so large images
    // stay within practical 2D texture dimensions.
    const float tileW = float(m_dims[0] + 2);
    const float tileH = float(m_dims[1] + 2);
    const uint32_t slices = m_dims[2] + 2;
    const float idealTilesX = std::sqrt(float(slices) * tileH / tileW);
    m_layout = {};
    m_layout.tilesX =
        std::clamp(uint32_t(std::lround(idealTilesX)), 1u, slices);
    m_layout.tilesY = (slices + m_layout.tilesX - 1) / m_layout.tilesX;
    for (int i = 0; i < 3; i++)
      m_layout.wrap[i] = m_wrapMode[i];
    for (int i = 0; i < 4; i++)
      m_layout.borderColor[i] = m_borderColor[i];

    auto &state = *deviceState();
    auto loader =
        std::make_unique<SamplerImageLoader>(m_image.get(), m_layout);
    ccl::ImageParams params;
    params.alpha_type = IMAGE_ALPHA_AUTO;
    params.colorspace = imageColorspace(m_image->elementType());
    // Coordinates are wrapped and kept inside the padded tile by the graph
    params.extension = EXTENSION_EXTEND;
    params.interpolation =
        m_linearFilter ? INTERPOLATION_LINEAR : INTERPOLATION_CLOSEST;
    m_handle =
        state.scene->image_manager->add_image(std::move(loader), params, false);
  }
  // notify observing materials so they rebuild their graphs on the new handle
  Object::finalize();
}

Sampler::SamplerOutputs Image3D::createNodeGraph(ccl::ShaderGraph *graph)
{
  if (!graph || m_handle.empty())
    return {};

  auto math = [&](ccl::NodeMathType type,
                  ccl::ShaderOutput *v1,
                  ccl::ShaderOutput *v2,
                  float c2 = 0.f) -> ccl::ShaderOutput * {
    auto *n = graph->create_node<ccl::MathNode>();
    n->set_math_type(type);
    graph->connect(v1, n->input("Value1"));
    if (v2)
      graph->connect(v2, n->input("Value2"));
    else
      n->set_value2(c2);
    return n->output("Value");
  };
  auto mad = [&](ccl::ShaderOutput *a,
                 ccl::ShaderOutput *b,
                 float bConst,
                 ccl::ShaderOutput *c,
                 float cConst) -> ccl::ShaderOutput * {
    auto *n = graph->create_node<ccl::MathNode>();
    n->set_math_type(ccl::NODE_MATH_MULTIPLY_ADD);
    graph->connect(a, n->input("Value1"));
    if (b)
      graph->connect(b, n->input("Value2"));
    else
      n->set_value2(bConst);
    if (c)
      graph->connect(c, n->input("Value3"));
    else
      n->set_value3(cConst);
    return n->output("Value");
  };

  auto in = makeAttributeInput(graph, m_inAttribute);
  auto uvw = applyAffineTransform(graph, in, m_inTransform, m_inOffset);

  auto *separate = graph->create_node<ccl::SeparateXYZNode>();
  graph->connect(uvw.color, separate->input("Vector"));
  const char *axisNames[3] = {"X", "Y", "Z"};

  // Wrapped coordinate in padded texel units of one axis (texel i of the
  // padded axis is centered at i + 0.5, source texel j is padded texel j + 1),
  // clamped to the padding texel centers: p = clamp(wrap(u) * n + 1, 0.5,
  // n + 1.5). Clamping onto the padding (which holds what the wrap mode
  // samples just outside the image) reproduces clampToEdge/clampToBorder
  // beyond it and keeps bilinear taps inside the tile.
  ccl::ShaderOutput *padded[3];
  for (int a = 0; a < 3; a++) {
    ccl::ShaderOutput *u = separate->output(axisNames[a]);
    if (m_wrapMode[a] == SamplerWrap::REPEAT)
      u = math(ccl::NODE_MATH_FRACTION, u, nullptr);
    else if (m_wrapMode[a] == SamplerWrap::MIRROR_REPEAT)
      u = math(ccl::NODE_MATH_PINGPONG, u, nullptr, 1.f);
    const float n = float(m_dims[a]);
    auto *p = mad(u, nullptr, n, nullptr, 1.f);
    p = math(ccl::NODE_MATH_MAXIMUM, p, nullptr, 0.5f);
    padded[a] = math(ccl::NODE_MATH_MINIMUM, p, nullptr, n + 1.5f);
  }

  const float tileW = float(m_dims[0] + 2);
  const float tileH = float(m_dims[1] + 2);
  const float atlasW = float(m_layout.tilesX) * tileW;
  const float atlasH = float(m_layout.tilesY) * tileH;
  const float tilesX = float(m_layout.tilesX);

  // Atlas lookup of one padded Z slice (an integer-valued graph output)
  auto sampleSlice = [&](ccl::ShaderOutput *slice) -> ColorAlpha {
    auto *tileX = math(ccl::NODE_MATH_MODULO, slice, nullptr, tilesX);
    // +0.5 keeps the floor robust against division round-off
    auto *tileY = math(ccl::NODE_MATH_FLOOR,
        math(ccl::NODE_MATH_DIVIDE,
            math(ccl::NODE_MATH_ADD, slice, nullptr, 0.5f),
            nullptr,
            tilesX),
        nullptr);
    auto *u = math(ccl::NODE_MATH_DIVIDE,
        mad(tileX, nullptr, tileW, padded[0], 0.f),
        nullptr,
        atlasW);
    auto *v = math(ccl::NODE_MATH_DIVIDE,
        mad(tileY, nullptr, tileH, padded[1], 0.f),
        nullptr,
        atlasH);
    auto *uv = graph->create_node<ccl::CombineXYZNode>();
    graph->connect(u, uv->input("X"));
    graph->connect(v, uv->input("Y"));

    auto *tex = graph->create_node<ccl::ImageTextureNode>();
    tex->handle = m_handle;
    tex->set_colorspace(ccl::u_colorspace_auto);
    tex->set_extension(EXTENSION_EXTEND);
    tex->set_interpolation(
        m_linearFilter ? INTERPOLATION_LINEAR : INTERPOLATION_CLOSEST);
    graph->connect(uv->output("Vector"), tex->input("Vector"));
    return {tex->output("Color"), tex->output("Alpha")};
  };

  ColorAlpha sampled;
  if (!m_linearFilter) {
    // padded Z in [0.5, nz + 1.5] -> slice floor(p) in [0, nz + 1]
    sampled = sampleSlice(math(ccl::NODE_MATH_FLOOR, padded[2], nullptr));
  } else {
    // zc = p - 0.5 in [0, nz + 1]; s0 = min(floor(zc), nz); t = zc - s0
    auto *zc = math(ccl::NODE_MATH_SUBTRACT, padded[2], nullptr, 0.5f);
    auto *s0 = math(ccl::NODE_MATH_MINIMUM,
        math(ccl::NODE_MATH_FLOOR, zc, nullptr),
        nullptr,
        float(m_dims[2]));
    auto *s1 = math(ccl::NODE_MATH_ADD, s0, nullptr, 1.f);
    auto *t = math(ccl::NODE_MATH_SUBTRACT, zc, s0);

    const ColorAlpha c0 = sampleSlice(s0);
    const ColorAlpha c1 = sampleSlice(s1);

    // color = c0 + t * (c1 - c0)
    auto *dc = graph->create_node<ccl::VectorMathNode>();
    dc->set_math_type(ccl::NODE_VECTOR_MATH_SUBTRACT);
    graph->connect(c1.color, dc->input("Vector1"));
    graph->connect(c0.color, dc->input("Vector2"));
    auto *scaled = graph->create_node<ccl::VectorMathNode>();
    scaled->set_math_type(ccl::NODE_VECTOR_MATH_SCALE);
    graph->connect(dc->output("Vector"), scaled->input("Vector1"));
    graph->connect(t, scaled->input("Scale"));
    auto *color = graph->create_node<ccl::VectorMathNode>();
    color->set_math_type(ccl::NODE_VECTOR_MATH_ADD);
    graph->connect(c0.color, color->input("Vector1"));
    graph->connect(scaled->output("Vector"), color->input("Vector2"));

    // alpha = t * (a1 - a0) + a0
    auto *alpha = mad(t,
        math(ccl::NODE_MATH_SUBTRACT, c1.alpha, c0.alpha),
        0.f,
        c0.alpha,
        0.f);

    sampled = {color->output("Vector"), alpha};
  }

  auto out = applyAffineTransform(graph, sampled, m_outTransform, m_outOffset);
  return makeStandardOutputs(graph, out);
}

} // namespace anari_cycles
