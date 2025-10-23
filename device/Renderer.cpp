// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "Renderer.h"
#include "Array.h"

// cycles
#include "scene/background.h"
#include "scene/shader_nodes.h"
#include "scene/image_oiio.h"

#include <memory>

namespace anari_cycles {

static Renderer* make_renderer(std::string_view subtype, CyclesGlobalState* d)
{
    auto splitString = [](const std::string& input,
        const std::string& delim) -> std::vector<std::string> {
            std::vector<std::string> tokens;
            size_t pos = 0;
            while (true) {
                size_t begin = input.find_first_not_of(delim, pos);
                if (begin == input.npos)
                    return tokens;
                size_t end = input.find_first_of(delim, begin);
                tokens.push_back(input.substr(
                    begin, (end == input.npos) ? input.npos : (end - begin)));
                pos = end;
            }
    };

    //if (subtype == "raycast")
    //    return new Raycast(d);
    //else if (subtype == "ao")
    //    return new AmbientOcclusion(d);
    //else if (subtype == "diffuse_pathtracer" || subtype == "dpt")
    //    return new DiffusePathTracer(d);
    //else if (subtype == "directLight" || subtype == "default")
    //    return new DirectLight(d);
    //else if (subtype == "test")
    //    return new Test(d);
    //else if (beginsWith(subtype, "debug")) {
    //    auto* retval = new Debug(d);
    //    auto names = splitString(std::string(subtype), "_");
    //    if (names.size() > 1)
    //        retval->setParam("method", ANARI_STRING, names[1].c_str());
    //    return retval;
    //}
    //else
    return new Renderer(d, 1.0f);
}

Renderer::Renderer(CyclesGlobalState *s, float defaultAmbientRadiance) :
    Object(ANARI_RENDERER, s),
    m_backgroundImage(this),
    m_defaultAmbientRadiance(defaultAmbientRadiance)
{
  //commitParameters();
    m_ambientIntensity = defaultAmbientRadiance;
    helium::BaseObject::markParameterChanged();
    s->commitBuffer.addObjectToCommit(this);
}

Renderer::~Renderer()
{
    cleanup();
}

void Renderer::cleanup()
{
    if (m_backgroundImage) {
        //if (m_backgroundTexture) {
        //    cudaDestroyTextureObject(m_backgroundTexture);
        //    m_backgroundImage->releaseCUDAArrayUint8();
        //}
    }
}

void Renderer::commitParameters()
{
#if 0
  m_backgroundColor =
      getParam<anari_vec::float4>("background", {0.f, 0.f, 0.f, 1.f});
  m_ambientColor = getParam<anari_vec::float3>("ambientColor", {1.f, 1.f, 1.f});
  m_ambientIntensity = 0.1f * getParam<float>("ambientRadiance", 1.f);
  m_runAsync = getParam<bool>("runAsync", false);
#endif

  m_backgroundImage = getParamObject<Array2D>("background");
  m_bgColor = getParam<anari_vec::vec4>("background", { 0.f, 0.f, 0.f, 1.f });
  m_spp = getParam<int>("pixelSamples", 1);
  m_maxRayDepth = getParam<int>("maxRayDepth", 5);
  m_ambientColor = getParam<anari_vec::vec3>("ambientColor", { 1.f, 1.f, 1.f });
  m_ambientIntensity =
      getParam<float>("ambientRadiance", m_defaultAmbientRadiance);
  m_occlusionDistance = getParam<float>("ambientOcclusionDistance", 1e20f);
  m_checkerboard = getParam<bool>("checkerboarding", false);
  m_denoise = getParam<bool>("denoise", false);
  m_sampleLimit = getParam<int>("sampleLimit", 128);
  m_cullTriangleBF = getParam<bool>("cullTriangleBackfaces", false);
  m_volumeSamplingRate =
      std::clamp(getParam<float>("volumeSamplingRate", 0.125f), 1e-3f, 10.f);
  if (m_checkerboard)
      m_spp = 1;
}

int Renderer::spp() const
{
    return m_spp;
}

bool Renderer::checkerboarding() const
{
    return m_checkerboard;
}

bool Renderer::denoise() const
{
    return m_denoise;
}

int Renderer::sampleLimit() const
{
    return m_sampleLimit;
}

Renderer* Renderer::createInstance(
    std::string_view subtype, CyclesGlobalState* d)
{
    Renderer* retval = nullptr;

    auto* overrideType = getenv("CYCLES_OVERRIDE_RENDERER");

    if (overrideType != nullptr)
        subtype = overrideType;

    retval = make_renderer(subtype, d);

    return retval;
}

void Renderer::makeRendererCurrent() const
{
  auto &state = *deviceState();

  if (state.bg_color_node) {
      auto bgc = m_bgColor;
      state.bg_color_node->set_color(ccl::make_float3(bgc[0], bgc[1], bgc[2]));
      //state.bg_color_node->set_b(ccl::make_float3(bgc[0], bgc[1], bgc[2]));
      //auto ramp = state.bg_color_node->get_ramp();
      //auto ramp_a = state.bg_color_node->get_ramp_alpha();

      if (m_backgroundImage) {
          const anari_vec::vec4* data = m_backgroundImage->dataAs<anari_vec::vec4>();
          linalg::aliases::uint2 dims = m_backgroundImage->size();

          size_t count = (size_t)dims[0] * (size_t)dims[1];
          if(count > 0)
            state.bg_color_node->set_color(ccl::make_float3(data[count-1][0], data[count - 1][1], data[count - 1][2]));
      }

      //    int count = ccl::min((int)ramp.size(), (int)dims[0] * (int)dims[1]);
      //    for (int i = 0; i < count; i++) {
      //        ccl::float3& c = ramp[count - 1 - i];
      //        c.x = data[i][0];
      //        c.y = data[i][1];
      //        c.z = data[i][2];

      //        ramp_a[i] = data[i][3];
      //    }
      //}
      //else {
      //    for (ccl::float3& c : ramp) {
      //        c.x = bgc[0];
      //        c.y = bgc[1];
      //        c.z = bgc[2];
      //    }
      //}

      //state.bg_color_node->set_ramp(ramp);
      //state.bg_color_node->set_ramp_alpha(ramp_a);
  }

  if (state.ambientIntensity) {
      float ai = m_ambientIntensity;
      state.ambientIntensity->set_strength(ai / 4.0); //TODO:MJ
  }

  //if (m_backgroundImage && state.backgroundImage) {
  //    //auto bi = m_backgroundImage;
  //    //state.backgroundImage->set_strength(ai);
  //    const anari_vec::vec4* data = m_backgroundImage->dataAs<anari_vec::vec4>();
  //    std::string filename;
  //    ImageMetaData attr;
  //    linalg::aliases::uint2 dims = m_backgroundImage->size();
  //    attr.width = dims[0];
  //    attr.height = dims[1];
  //    attr.channels = 4;
  //    attr.depth = 1;
  //    attr.type = IMAGE_DATA_TYPE_FLOAT4;
  //    vector<char> image_buffer(attr.width * attr.height * attr.channels * attr.depth * sizeof(float));
  //    memcpy(image_buffer.data(), data, image_buffer.size());

  //    unique_ptr<ImageLoader> loader = std::make_unique<ccl::OIIOImageLoader>(filename, image_buffer, attr);
  //    ImageParams params;// = state.backgroundImage->image_params();
  //    ImageManager* image_manager = state.scene->image_manager.get();

  //    state.backgroundImage->handle = image_manager->add_image(std::move(loader), params, false);

  //    if (state.bg_color_node) {
  //        state.bg_color_node->set_a(ccl::make_float3(0, 0, 0));
  //        state.bg_color_node->set_b(ccl::make_float3(0, 0, 0));
  //    }
  //}

  

#if 0
  auto bgc = m_backgroundColor;

  for (auto &v : bgc)
    v = std::max(1e-6f, v);

  if (state.ambient) {
      state.ambient->set_color(ccl::make_float3(
          m_ambientColor[0], m_ambientColor[1], m_ambientColor[2]));
      state.ambient->set_strength(m_ambientIntensity);
  }

  if (state.background) {
      state.background->set_color(ccl::make_float3(bgc[0], bgc[1], bgc[2]));
      state.background->set_strength(1.f);
  }
#endif

  if (m_spp > 0) {
      state.session_params.samples = m_spp;
  }

  state.scene->default_background->tag_update(state.scene);
  state.scene->background->tag_update(state.scene);
}

#if 0
bool Renderer::runAsync() const
{
  return m_runAsync;
}
#endif

} // namespace anari_cycles

CYCLES_ANARI_TYPEFOR_DEFINITION(anari_cycles::Renderer *);
