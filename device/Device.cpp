// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#include "Device.h"
// anari
#include "anari/anari_cpp.hpp"
// cycles
#include "scene/background.h"
#include "scene/integrator.h"
#include "scene/shader_nodes.h"

#include <util/path.h>
#include <app/cycles_xml_bin.h>

#include "Array.h"
#include "Frame.h"

#include "FrameOutputDriver.h"

namespace anari_cycles {

///////////////////////////////////////////////////////////////////////////////
// Helper functions ///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//template <typename HANDLE_T, typename OBJECT_T>
//inline HANDLE_T getHandleForAPI(OBJECT_T *object)
//{
//  return (HANDLE_T)object;
//}
//
//template <typename OBJECT_T, typename HANDLE_T, typename... Args>
//inline HANDLE_T createObjectForAPI(CyclesGlobalState *s, Args &&...args)
//{
//  return getHandleForAPI<HANDLE_T>(
//      new OBJECT_T(s, std::forward<Args>(args)...));
//}

///////////////////////////////////////////////////////////////////////////////
// CyclesDevice definitions ///////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Data Arrays ////////////////////////////////////////////////////////////////

void *CyclesDevice::mapArray(ANARIArray a)
{
  deviceState()->waitOnCurrentFrame();
  return helium::BaseDevice::mapArray(a);
}

// API Objects ////////////////////////////////////////////////////////////////

ANARIArray1D CyclesDevice::newArray1D(const void *appMemory,
    ANARIMemoryDeleter deleter,
    const void *userData,
    ANARIDataType type,
    uint64_t numItems)
{
  initDevice();

  Array1DMemoryDescriptor md;
  md.appMemory = appMemory;
  md.deleter = deleter;
  md.deleterPtr = userData;
  md.elementType = type;
  md.numItems = numItems;

  //if (anari::isObject(type))
  //  return createObjectForAPI<ObjectArray, ANARIArray1D>(deviceState(), md);
  //else
  //  return createObjectForAPI<Array1D, ANARIArray1D>(deviceState(), md);

  if (anari::isObject(type))
      return (ANARIArray1D) new ObjectArray(deviceState(), md);
  else
      return (ANARIArray1D) new Array1D(deviceState(), md);
}

ANARIArray2D CyclesDevice::newArray2D(const void *appMemory,
    ANARIMemoryDeleter deleter,
    const void *userData,
    ANARIDataType type,
    uint64_t numItems1,
    uint64_t numItems2)
{
  initDevice();

  Array2DMemoryDescriptor md;
  md.appMemory = appMemory;
  md.deleter = deleter;
  md.deleterPtr = userData;
  md.elementType = type;
  md.numItems1 = numItems1;
  md.numItems2 = numItems2;

  //return createObjectForAPI<Array2D, ANARIArray2D>(deviceState(), md);
  return (ANARIArray2D) new Array2D(deviceState(), md);
}

ANARIArray3D CyclesDevice::newArray3D(const void *appMemory,
    ANARIMemoryDeleter deleter,
    const void *userData,
    ANARIDataType type,
    uint64_t numItems1,
    uint64_t numItems2,
    uint64_t numItems3)
{
  initDevice();

  Array3DMemoryDescriptor md;
  md.appMemory = appMemory;
  md.deleter = deleter;
  md.deleterPtr = userData;
  md.elementType = type;
  md.numItems1 = numItems1;
  md.numItems2 = numItems2;
  md.numItems3 = numItems3;

  //return createObjectForAPI<Array3D, ANARIArray3D>(deviceState(), md);
  return (ANARIArray3D) new Array3D(deviceState(), md);
}

ANARICamera CyclesDevice::newCamera(const char *subtype)
{
  initDevice();
  //return getHandleForAPI<ANARICamera>(
  //    Camera::createInstance(subtype, deviceState()));
  return (ANARICamera)Camera::createInstance(subtype, deviceState());
}

ANARIFrame CyclesDevice::newFrame()
{
  initDevice();
  //return createObjectForAPI<Frame, ANARIFrame>(deviceState());
  return (ANARIFrame) new Frame(deviceState());
}

ANARIGeometry CyclesDevice::newGeometry(const char *subtype)
{
  initDevice();
  //return getHandleForAPI<ANARIGeometry>(
  //    Geometry::createInstance(subtype, deviceState()));
  return (ANARIGeometry)Geometry::createInstance(subtype, deviceState());
}

ANARIGroup CyclesDevice::newGroup()
{
  initDevice();
  //return createObjectForAPI<Group, ANARIGroup>(deviceState());
  return (ANARIGroup) new Group(deviceState());
}

ANARIInstance CyclesDevice::newInstance(const char * /*subtype*/)
{
  initDevice();
  //return createObjectForAPI<Instance, ANARIInstance>(deviceState());
  return (ANARIInstance) new Instance(deviceState());
}

ANARILight CyclesDevice::newLight(const char *subtype)
{
  initDevice();
  //return getHandleForAPI<ANARILight>(
  //    Light::createInstance(subtype, deviceState()));
  return (ANARILight)Light::createInstance(subtype, deviceState());
}

ANARIMaterial CyclesDevice::newMaterial(const char *subtype)
{
  initDevice();
  //return getHandleForAPI<ANARIMaterial>(
  //    Material::createInstance(subtype, deviceState()));
  return (ANARIMaterial)Material::createInstance(subtype, deviceState());
}

ANARIRenderer CyclesDevice::newRenderer(const char *subtype)
{
  initDevice();
  //return createObjectForAPI<Renderer, ANARIRenderer>(deviceState());
  return (ANARIRenderer)Renderer::createInstance(subtype, deviceState());
}

ANARISampler CyclesDevice::newSampler(const char *subtype)
{
  initDevice();
  //return getHandleForAPI<ANARISampler>(
  //    Sampler::createInstance(subtype, deviceState()));
  return (ANARISampler)Sampler::createInstance(subtype, deviceState());
}

ANARISpatialField CyclesDevice::newSpatialField(const char *subtype)
{
  initDevice();
  //return getHandleForAPI<ANARISpatialField>(
  //    SpatialField::createInstance(subtype, deviceState()));
  return (ANARISpatialField)SpatialField::createInstance(
      subtype, deviceState());
}

ANARISurface CyclesDevice::newSurface()
{
  initDevice();
  //return createObjectForAPI<Surface, ANARISurface>(deviceState());
  return (ANARISurface) new Surface(deviceState());
}

ANARIVolume CyclesDevice::newVolume(const char *subtype)
{
  initDevice();
  //return getHandleForAPI<ANARIVolume>(
  //    Volume::createInstance(subtype, deviceState()));
  return (ANARIVolume)Volume::createInstance(subtype, deviceState());
}

ANARIWorld CyclesDevice::newWorld()
{
  initDevice();
  //return createObjectForAPI<World, ANARIWorld>(deviceState());
  return (ANARIWorld) new World(deviceState());
}

// Query functions ////////////////////////////////////////////////////////////

const char **CyclesDevice::getObjectSubtypes(ANARIDataType objectType)
{
  return anari_cycles::query_object_types(objectType);
}

const void *CyclesDevice::getObjectInfo(ANARIDataType objectType,
    const char *objectSubtype,
    const char *infoName,
    ANARIDataType infoType)
{
  return anari_cycles::query_object_info(
      objectType, objectSubtype, infoName, infoType);
}

const void *CyclesDevice::getParameterInfo(ANARIDataType objectType,
    const char *objectSubtype,
    const char *parameterName,
    ANARIDataType parameterType,
    const char *infoName,
    ANARIDataType infoType)
{
  return anari_cycles::query_param_info(objectType,
      objectSubtype,
      parameterName,
      parameterType,
      infoName,
      infoType);
}

// Object + Parameter Lifetime Management /////////////////////////////////////

int CyclesDevice::getProperty(ANARIObject object,
    const char *name,
    ANARIDataType type,
    void *mem,
    uint64_t size,
    uint32_t mask)
{
  if (mask == ANARI_WAIT) {
    auto lock = scopeLockObject();
#if 0 // TODO: this causes a crash in anariViewer...(???)
    deviceState()->waitOnCurrentFrame();
#endif
  }

  return helium::BaseDevice::getProperty(object, name, type, mem, size, mask);
}

// Other CyclesDevice definitions /////////////////////////////////////////////

CyclesDevice::CyclesDevice(ANARIStatusCallback cb, const void *ptr)
    : helium::BaseDevice(cb, ptr)
{
  m_state = std::make_unique<CyclesGlobalState>(this_device());
  deviceCommitParameters();
}

CyclesDevice::CyclesDevice(ANARILibrary l) : helium::BaseDevice(l)
{
  m_state = std::make_unique<CyclesGlobalState>(this_device());
  deviceCommitParameters();
}

CyclesDevice::~CyclesDevice()
{
  if (m_initialized) {
    auto &state = *deviceState();
    state.session->cancel(true);
    state.session->wait();
    state.commitBuffer.clear();
  }

  reportMessage(ANARI_SEVERITY_DEBUG, "destroyed cycles device (%p)", this);
}

int CyclesDevice::deviceGetProperty(const char *name,
    ANARIDataType type,
    void *mem,
    uint64_t size,
    uint32_t mask)
{
  std::string_view prop = name;
  if (prop == "feature" && type == ANARI_STRING_LIST) {
    helium::writeToVoidP(mem, query_extensions());
    return 1;
  } else if (prop == "cycles" && type == ANARI_BOOL) {
    helium::writeToVoidP(mem, true);
    return 1;
  }
  return 0;
}

void CyclesDevice::initDevice()
{
  if (m_initialized)
    return;

  reportMessage(ANARI_SEVERITY_DEBUG, "initializing cycles device (%p)", this);

  auto& state = *deviceState();

#if 0
  auto *forceCPU = getenv("ANARI_CYCLES_FORCE_CPU");

  auto devices = ccl::Device::available_devices();
  ccl::DeviceInfo selectedDevice;
  for (ccl::DeviceInfo &info : devices) {
    reportMessage(ANARI_SEVERITY_INFO,
        "Found Cycles Device: %-7s| %s",
        ccl::Device::string_from_type(info.type).c_str(),
        info.description.c_str());
    if (!forceCPU && info.type == ccl::DEVICE_OPTIX)
      selectedDevice = info;
    else if (!forceCPU && selectedDevice.type != ccl::DEVICE_OPTIX
        && info.type == ccl::DEVICE_CUDA)
      selectedDevice = info;
  } 
  state.session_params.device = selectedDevice;
#else
  auto *useGPU = getenv("CYCLES_ANARI_USE_GPU");
  //state.session_params.device.type = useGPU ? ccl::DEVICE_OPTIX : ccl::DEVICE_CPU;
  state.session_params.device.type = ccl::DEVICE_CPU;
  if (useGPU) {
      printf("useGPU: %s\n", useGPU);

      /* find matching device */
      ccl::DeviceType device_type = ccl::Device::type_from_string(useGPU);
      std::vector<ccl::DeviceInfo> devices = ccl::Device::available_devices((ccl::DeviceTypeMask)(1 << device_type));

      bool device_available = false;
      if (!devices.empty()) {
          state.session_params.device = devices.front();
          device_available = true;
      }

      /* handle invalid configurations */
      if (state.session_params.device.type == ccl::DEVICE_NONE || !device_available) {
          fprintf(stderr, "Unknown device: %s\n", useGPU);
          exit(-1);
      }
  }
#endif
  state.session_params.background = false;
  state.session_params.headless = false;
  state.session_params.use_auto_tile = false;
  state.session_params.tile_size = 2048;
  state.session_params.use_resolution_divider = false;
  state.session_params.samples = 1;

  // TODO:MJ
  //state.session_params.threads = 1;

  reportMessage(ANARI_SEVERITY_INFO,
      "Using Cycles Device '%s'",
      ccl::Device::string_from_type(state.session_params.device.type).c_str());

  state.session =
      std::make_unique<ccl::Session>(state.session_params, state.scene_params);
  state.scene = state.session->scene.get();

  // We cannot use adaptive sampling based on ANARI's async execution model,
  // me _must_ know that the next sample will get executed to trigger completion
  // code signaling the frame is complete.
  state.scene->integrator->set_use_adaptive_sampling(false);

  ccl::Pass *pass_combined = state.scene->create_node<ccl::Pass>();
  pass_combined->set_name(OIIO::ustring("combined"));
  pass_combined->set_type(ccl::PASS_COMBINED);

  ccl::Pass *pass_depth = state.scene->create_node<ccl::Pass>();
  pass_depth->set_name(OIIO::ustring("depth"));
  pass_depth->set_type(ccl::PASS_DEPTH);

  auto output_driver = std::make_unique<FrameOutputDriver>();
  state.output_driver = output_driver.get();

  state.session->set_output_driver(std::move(output_driver));

  // set scene
  std::string filepath_xml;
  const char* env_xml = getenv("CYCLES_XML_PATH");
  if (env_xml) {
      filepath_xml = std::string(env_xml);
  }
  else {
      filepath_xml = path_join(path_get("anari"), "cycles_default_scene.xml");
  }

  xml_read_file(state.scene, filepath_xml.c_str());

  //TODO:MJ
  if (state.scene->default_background) {
      for (ShaderNode* node : state.scene->default_background->graph->nodes) {
          if (node->name == "bgColor" && node->type == BackgroundNode::get_node_type()) {
              state.bg_color_node = (ccl::BackgroundNode*)node;
          }
          else 
          if (node->name == "ambientIntensity" && node->type == BackgroundNode::get_node_type()) {
              state.ambientIntensity = (ccl::BackgroundNode*)node;
          }
/*          else if (node->name == "backgroundImage") {
              state.backgroundImage = (ImageTextureNode*)node;
          } */         
      }
  }

#if 0
  else 
  {
    // setup background shader (divides out ambient and bg color)
    {  
      auto *shader = state.scene->default_background;
      auto graph = std::make_unique<ccl::ShaderGraph>();
      auto *mix = graph->create_node<ccl::MixClosureNode>();
      auto *lightPath = graph->create_node<ccl::LightPathNode>();
      auto *bg = graph->create_node<ccl::BackgroundNode>();
      bg->name = "background_shader";
      auto *ambient = graph->create_node<ccl::BackgroundNode>();
      ambient->name = "ambient_shader";

      //state.background = bg;
      //state.ambient = ambient;

      graph->connect(ambient->output("Background"), mix->input("Closure1"));
      graph->connect(bg->output("Background"), mix->input("Closure2"));
      graph->connect(lightPath->output("Is Camera Ray"), mix->input("Fac"));
      graph->connect(mix->output("Closure"), graph->output()->input("Surface"));

      shader->set_graph(std::move(graph));

      state.scene->background->set_shader(state.scene->default_background);
      state.scene->background->set_use_shader(true);
    }

    // setup global light shader
    {
      auto *shader = state.scene->default_light;
      auto graph = std::make_unique<ccl::ShaderGraph>();

      auto *emission = graph->create_node<ccl::EmissionNode>();
      emission->set_color(make_float3(1.f, 1.f, 1.f));
      emission->set_strength(4.0f); // to match VisRTX

      graph->connect(
          emission->output("Emission"), graph->output()->input("Surface"));

      shader->name = "default_anari_light";
      shader->set_graph(std::move(graph));
      shader->reference();
    }
  }
#endif

  m_initialized = true;
}

CyclesGlobalState *CyclesDevice::deviceState() const
{
  return (CyclesGlobalState *)helium::BaseDevice::m_state.get();
}

} // namespace anari_cycles
