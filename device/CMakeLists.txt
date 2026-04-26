## Copyright 2025 Jefferson Amstutz
## SPDX-License-Identifier: Apache-2.0

## Establish Project ##

project(anari_library_cycles LANGUAGES CXX)

## Dependencies ##

find_package(anari 0.15.0 REQUIRED)

## Cycles definitions ##

add_definitions(
  -DCCL_NAMESPACE_BEGIN=namespace\ ccl\ {
  -DCCL_NAMESPACE_END=}
)

add_definitions(
	-DWITH_PUGIXML
	-DWITH_SYSTEM_PUGIXML
	-DWITH_OPENVDB
	-DWITH_NANOVDB
)

## Build device library ##

add_library(${PROJECT_NAME} SHARED)

include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME}
  EXPORT_MACRO_NAME "CYCLES_DEVICE_INTERFACE"
)

target_sources(${PROJECT_NAME}
PRIVATE
  CyclesGlobalState.cpp
  CyclesGlobalState.h

  #camera
  Camera.cpp
  Camera.h

  #device
  Device.cpp
  Device.h

  #frame
  Frame.cpp
  Frame.h
  FrameOutputDriver.cpp
  FrameOutputDriver.h

  #scene/surface/geometry
  Geometry.cpp
  Geometry.h
	Cone.cpp
	Cone.h
	Curve.cpp
	Curve.h
	Cylinder.cpp
	Cylinder.h
	Geometry.cpp
	Geometry.h
	Quad.cpp
	Quad.h
	Sphere.cpp
	Sphere.h
	Triangle.cpp
	Triangle.h

  #scene
  Group.cpp
  Group.h
  Instance.cpp
  Instance.h  

  #scene/light
  Light.cpp
  Light.h

  #scene/surface/material/
  Material.cpp
  Material.h

  #library
  Library.cpp

  #object
  Object.cpp
  Object.h  

  #renderer
  Renderer.cpp
  Renderer.h

  #scene/surface/material/sampler
  Sampler.cpp
  Sampler.h
  SamplerImageLoader.cpp
  SamplerImageLoader.h

  #scene/volume/spatial_field/
  SpatialField.cpp
  SpatialField.h

  #scene/surface
  Surface.cpp
  Surface.h

  #scene/volume/
  Volume.cpp
  Volume.h
  VolumeImageLoader.cpp
  VolumeImageLoader.h

  #scene/
  World.cpp
  World.h

  #array
  Array.h

  #utility
  cycles_math.h
  colorMapHelpers.h

  ${CMAKE_SOURCE_DIR}/cyclesphi/src/app/cycles_xml_bin.cpp
  ${CMAKE_SOURCE_DIR}/cyclesphi/src/app/cycles_xml_bin.h
  ${CMAKE_SOURCE_DIR}/cyclesphi/src/app/cycles_xml_nodes.cpp
  ${CMAKE_SOURCE_DIR}/cyclesphi/src/app/cycles_xml_nodes.h
)

get_target_property(CYCLES_INCLUDE_DIRECTORIES cycles_bvh INCLUDE_DIRECTORIES)

target_include_directories(${PROJECT_NAME}
PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CYCLES_INCLUDE_DIRECTORIES}
  ${CMAKE_SOURCE_DIR}/cyclesphi/src
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

if(ANARI_CYCLES_USE_OPTIX)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_OPTIX)
endif()
if(ANARI_CYCLES_USE_OIDN)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_OPENIMAGEDENOISE)
endif()

###################LIB######################
set(LIB
  cycles_device
  cycles_kernel
  cycles_scene
  cycles_session
  cycles_bvh
  cycles_subd
  cycles_graph
  cycles_util

  ${ALEMBIC_LIBRARIES}
  ${BOOST_LIBRARIES}
  ${EMBREE_LIBRARIES}
  ${Epoxy_LIBRARIES}
  ${OPENCOLORIO_LIBRARIES}
  ${OPENEXR_LIBRARIES}
  ${OPENIMAGEIO_LIBRARIES}
  ${OPENJPEG_LIBRARIES}
  ${OPENPGL_LIBRARIES}
  ${OPENSUBDIV_LIBRARIES}
  ${OSL_LIBRARIES}
  ${PTHREADS_LIBRARIES}
  ${PUGIXML_LIBRARIES}
  ${PYTHON_LIBRARIES}
  ${TBB_LIBRARIES}
  ${USD_LIBRARIES}
  ${ZLIB_LIBRARIES}
  ${ZSTD_LIBRARIES}
)

target_link_libraries(${PROJECT_NAME}
PRIVATE
  anari::helium
  anari::anari
  ${LIB}
)

# Link the Windows version library
if (WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE version)
endif()

## ANARI query code generation ##

anari_generate_queries(
  DEVICE_TARGET ${PROJECT_NAME}
  CPP_NAMESPACE anari_cycles
  JSON_DEFINITIONS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cycles_device.json
)

## Installation ##

install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
