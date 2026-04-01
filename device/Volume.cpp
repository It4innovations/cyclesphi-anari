// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "Volume.h"
// std
#include <numeric>
// cycles
//#include "graph/node_xml.h"
#include "scene/shader_nodes.h"
#include "scene/volume.h"
#include "util/path.h"
#include <app/cycles_xml_bin.h>

// anari
#include "colorMapHelpers.h"

namespace anari_cycles {

Volume::Volume(CyclesGlobalState *s) : Object(ANARI_VOLUME, s) {}

Volume::~Volume() = default;

Volume *Volume::createInstance(std::string_view subtype, CyclesGlobalState *s)
{
  if (subtype == "transferFunction1D")
    return new TransferFunction1D(s);
  else
    return (Volume *)new UnknownObject(ANARI_VOLUME, subtype, s);
}

// Subtypes ///////////////////////////////////////////////////////////////////

TransferFunction1D::TransferFunction1D(CyclesGlobalState *s)
    : Volume(s), m_color(this), m_opacity(this), m_field(this)
{
  m_shader = s->scene->create_node<ccl::Shader>();

  //auto shader = std::make_unique<ccl::Shader>();
  //m_shader = shader.get();
  //state.scene->shaders.push_back(std::move(shader));

//   auto m_graph = std::make_unique<ccl::ShaderGraph>();

//   m_volumeNode = m_graph->create_node<ccl::PrincipledVolumeNode>();
//   m_volumeNode->set_density_attribute(ustring("never-connected"));

//   m_attributeNode = m_graph->create_node<ccl::AttributeNode>();
//   m_attributeNode->set_attribute(ustring("density"));

//   m_mapRangeNode = m_graph->create_node<ccl::MapRangeNode>();
//   m_mapRangeNode->set_clamp(true);

//   m_rgbRampNode = m_graph->create_node<ccl::RGBRampNode>();

//   m_graph->connect(
//       m_volumeNode->output("Volume"), m_graph->output()->input("Volume"));
//   m_graph->connect(
//       m_attributeNode->output("Fac"), m_mapRangeNode->input("Value"));
//   m_graph->connect(
//       m_mapRangeNode->output("Result"), m_rgbRampNode->input("Fac"));
//   m_graph->connect(
//       m_rgbRampNode->output("Color"), m_volumeNode->input("Color"));
//   m_graph->connect(
//       m_rgbRampNode->output("Alpha"), m_volumeNode->input("Density"));

//   m_shader->set_graph(std::move(m_graph));
//   m_shader->tag_update(s->scene);

  const string xml_path = path_join(path_get("anari"), "transferFunction1D.xml");
  xml_set_material_to_shader2(deviceState()->scene, m_shader, xml_path.c_str());

  for (ShaderNode* node : m_shader->graph->nodes) {
      if (node->type == ccl::PrincipledVolumeNode::get_node_type()) {
          m_volumeNode = (ccl::PrincipledVolumeNode*)node;
      }
      else if (node->type == ccl::AttributeNode::get_node_type()) {
          m_attributeNode = (ccl::AttributeNode*)node;
      }
      else if (node->type == ccl::MapRangeNode::get_node_type()) {
          m_mapRangeNode = (ccl::MapRangeNode*)node;
      }
      else if (node->type == ccl::RGBRampNode::get_node_type()) {
          m_rgbRampNode = (ccl::RGBRampNode*)node;
      }
      else if (node->type == ccl::MathNode::get_node_type()) {
          m_mathNode = (ccl::MathNode*)node;
      }
  }

  //m_shader->has_volume = true;
}

TransferFunction1D::~TransferFunction1D()
{
  auto &state = *deviceState();
  state.scene->shaders.erase(cyclesShader());
}

ccl::Shader *TransferFunction1D::cyclesShader()
{
  return m_shader;
}

bool TransferFunction1D::isValid() const
{
  //return m_field && m_field->isValid() && m_colorData && m_opacityData;
  return m_field && m_field->isValid();
}

void TransferFunction1D::commitParameters()
{
  //Volume::commitParameters();

  //auto* prevField = m_field.ptr;
  //m_field = getParamObject<SpatialField>("value");
  //m_fieldHandleChanged = prevField != m_field.ptr;
  //if (!m_field) {
  //  reportMessage(ANARI_SEVERITY_WARNING,
  //      "no spatial field provided to transferFunction1D volume");
  //  return;
  //}

  //m_bounds = m_field->bounds();

  //m_valueRange = getParam<helium::box1>("valueRange", helium::box1{0.f, 1.f});

  //m_colorData = getParamObject<helium::Array1D>("color");
  //m_opacityData = getParamObject<helium::Array1D>("opacity");
  //m_densityScale = getParam<float>("unitDistance", 1.f);

  //if (!m_colorData) {
  //  reportMessage(ANARI_SEVERITY_WARNING,
  //      "no color data provided to transferFunction1D volume");
  //  return;
  //}

  //if (!m_opacityData) {
  //  reportMessage(ANARI_SEVERITY_WARNING,
  //      "no opacity data provided to transfer function");
  //  return;
  //}

  //if (m_mapRangeNode != nullptr) {
  //  m_mapRangeNode->set_from_min(m_valueRange.lower);
  //  m_mapRangeNode->set_from_max(m_valueRange.upper);
  //}

  //if (m_mathNode != nullptr) {
  //  m_mathNode->set_value2(m_densityScale);
  //}

  //// m_colorData, m_opacityData
  //auto *colorData = m_colorData->beginAs<anari_vec::float3>();
  //auto *opacityData = m_opacityData->beginAs<float>();

  //if (m_rgbRampNode != nullptr) {
  //  m_rgbRampNode->get_ramp().resize(m_colorData->size());
  //  m_rgbRampNode->get_ramp_alpha().resize(m_opacityData->size());

  //  for (size_t i = 0; i < m_colorData->size(); ++i) {
  //    m_rgbRampNode->get_ramp()[i] =
  //        (ccl::make_float3(colorData[i][0], colorData[i][1], colorData[i][2]));
  //  }

  //  for (size_t i = 0; i < m_opacityData->size(); ++i) {
  //    m_rgbRampNode->get_ramp_alpha()[i] = opacityData[i];
  //  }
  //}

  //m_shader->tag_update(deviceState()->scene);

    Volume::commitParameters();
    m_color = getParamObject<Array1D>("color");
    m_uniformColor = anari_vec::vec4{ 1.f };
    getParam("color", ANARI_FLOAT32_VEC3, &m_uniformColor);
    getParam("color", ANARI_FLOAT32_VEC4, &m_uniformColor);
    m_opacity = getParamObject<Array1D>("opacity");
    m_uniformOpacity = getParam<float>("opacity", 1.f) * m_uniformColor[3];
    m_unitDistance = getParam<float>("unitDistance", 1.f);
    
    //m_field = getParamObject<SpatialField>("value");
    auto *prevField = m_field.get();
    m_field = getParamObject<SpatialField>("value");
    m_fieldHandleChanged = prevField != m_field.get();

    getParam("valueRange", ANARI_FLOAT32_VEC2, &m_valueRange);
    getParam("valueRange", ANARI_FLOAT32_BOX1, &m_valueRange);
    double valueRange_d[2] = { 0.0, 1.0 };
    if (getParam("valueRange", ANARI_FLOAT64_BOX1, valueRange_d)) {
        m_valueRange.lower = float(valueRange_d[0]);
        m_valueRange.upper = float(valueRange_d[1]);
    }

    if (m_mapRangeNode != nullptr) {
      m_mapRangeNode->set_from_min(m_valueRange.lower);
      m_mapRangeNode->set_from_max(m_valueRange.upper);
    }

    // TODO:MJ (no node in xml)
    if (m_mathNode != nullptr) {
      m_mathNode->set_value2(m_unitDistance);
    }

#if 0

    //m_rgbRampNode->get_ramp()

    std::vector<ColorRampPoint> color_ramp;

    if (m_color) {
        if (m_color->elementType() == ANARI_FLOAT32_VEC3) {
            auto* colorData = m_color->beginAs<anari_vec::vec3>();
            color_ramp.resize(m_color->size());
            //m_rgbRampNode->get_ramp_alpha().clear();

            for (size_t i = 0; i < m_color->size(); ++i) {
                color_ramp[i].rgba =
                    ccl::make_float4(colorData[i][0], colorData[i][1], colorData[i][2], float(i) / float(m_color->size()));
            }
        }
        else if (m_color->elementType() == ANARI_FLOAT32_VEC4) {
            auto* colorData = m_color->beginAs<anari_vec::vec4>();
            //m_rgbRampNode->get_ramp().resize(m_color->size());
            //m_rgbRampNode->get_ramp_alpha().resize(m_color->size());
            color_ramp.resize(m_color->size());

            for (size_t i = 0; i < m_color->size(); ++i) {
                color_ramp[i].rgba =
                    ccl::make_float4(colorData[i][0], colorData[i][1], colorData[i][2], colorData[i][3]);
            }
        }
    }

    if (m_opacity) {
        auto* opacityData = m_opacity->beginAs<float>();
        color_ramp.resize(m_opacity->size());

        for (size_t i = 0; i < m_opacity->size(); ++i) {
            color_ramp[i].rgba.w = opacityData[i];
        }
    }

    if (m_rgbRampNode) {
        m_rgbRampNode->set_interpolate(false);
        colorramp_to_array(color_ramp,
            m_rgbRampNode->get_ramp(),
            m_rgbRampNode->get_ramp_alpha(),
            RAMP_TABLE_SIZE);
    }
#endif

    if (m_rgbRampNode) {
        m_rgbRampNode->set_interpolate(true);
    }

    if (m_color && m_rgbRampNode != nullptr) {
        if (m_color->elementType() == ANARI_FLOAT32_VEC3) {
            auto* colorData = m_color->beginAs<anari_vec::float3>();
            m_rgbRampNode->get_ramp().resize(m_color->size());
            m_rgbRampNode->get_ramp_alpha().resize(m_color->size());

            for (size_t i = 0; i < m_color->size(); ++i) {
                m_rgbRampNode->get_ramp()[i] =
                    (ccl::make_float3(colorData[i][0], colorData[i][1], colorData[i][2]));

                m_rgbRampNode->get_ramp_alpha()[i] = float(i)/float(m_color->size());
            }
        }
        else if (m_color->elementType() == ANARI_FLOAT32_VEC4) {
            auto* colorData = m_color->beginAs<anari_vec::float4>();
            m_rgbRampNode->get_ramp().resize(m_color->size());
            m_rgbRampNode->get_ramp_alpha().resize(m_color->size());

            for (size_t i = 0; i < m_color->size(); ++i) {
                m_rgbRampNode->get_ramp()[i] =
                    (ccl::make_float3(colorData[i][0], colorData[i][1], colorData[i][2]));

                m_rgbRampNode->get_ramp_alpha()[i] = colorData[i][3];
            }
        }
    }

    if (m_opacity && m_rgbRampNode != nullptr) {
        auto* opacityData = m_opacity->beginAs<float>();
        m_rgbRampNode->get_ramp_alpha().resize(m_opacity->size());

        for (size_t i = 0; i < m_opacity->size(); ++i) {
          if (i == 0)
            m_rgbRampNode->get_ramp_alpha()[i] =
                0.f; // force zero opacity at the start of the ramp to avoid
                     // artifacts when the first color is not fully transparent
          else
            m_rgbRampNode->get_ramp_alpha()[i] = opacityData[i];
        }
    }

    //TODO:MJ - disabled it - it call rebuild octree everytime
    //m_shader->tag_update(deviceState()->scene);
}

void TransferFunction1D::finalize() {
    if (!m_field) {
        reportMessage(ANARI_SEVERITY_WARNING,
            "missing parameter 'value' on transferFunction1D ANARIVolume");
        return;
    }

    auto* state = deviceState();    

    if (m_fieldHandleChanged) 
    {
        cleanupCyclesNode();
        if (m_field)
            m_cyclesGeometryNode = m_field->createCyclesGeometryNode();
    }

    if (isValid()) {        
        if (m_fieldHandleChanged)
        {
            m_field->syncCyclesNode(m_cyclesGeometryNode);

            ccl::array<ccl::Node*> used_shaders;
            used_shaders.push_back_slow(cyclesShader());
            m_cyclesGeometryNode->set_used_shaders(used_shaders);
        }
        m_cyclesGeometryNode->tag_update(state->scene, true);
    }

    m_fieldHandleChanged = false;

    state->objectUpdates.lastSceneChange = helium::newTimeStamp();

    Object::finalize();
}

//std::unique_ptr<ccl::Geometry> TransferFunction1D::makeCyclesGeometry()
//{
//  auto g = m_field->makeCyclesGeometry();
//  ccl::array<ccl::Node *> used_shaders;
//  used_shaders.push_back_slow(cyclesShader());
//  g->set_used_shaders(used_shaders);
//  return g;
//}

//box3 TransferFunction1D::bounds() const
//{
//  return m_field->bounds();
//}

ccl::Geometry* TransferFunction1D::cyclesGeometry() const
{
    return m_cyclesGeometryNode;
}

void TransferFunction1D::cleanupCyclesNode()
{
    auto& state = *deviceState();
    if (auto* cg = cyclesGeometry(); cg != nullptr)
        state.scene->delete_node(cg);
    m_cyclesGeometryNode = nullptr;
}

// void TransferFunction1D::cleanup() {}

} // namespace anari_cycles

CYCLES_ANARI_TYPEFOR_DEFINITION(anari_cycles::Volume *);
