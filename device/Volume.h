// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

// #include "Array.h"
#include "Object.h"
#include "SpatialField.h"

namespace anari_cycles {

struct Volume : public Object
{
  Volume(CyclesGlobalState *s);
  ~Volume() override;

  static Volume *createInstance(std::string_view subtype, CyclesGlobalState *s);

  //virtual std::unique_ptr<ccl::Geometry> makeCyclesGeometry() = 0;
  //virtual ccl::Geometry* createCyclesGeometryNode() = 0;
  //virtual void syncCyclesNode(ccl::Geometry* node) const = 0;
  virtual ccl::Geometry* cyclesGeometry() const = 0;

  //virtual box3 bounds() const = 0;
};

// Subtypes ///////////////////////////////////////////////////////////////////

struct TransferFunction1D : public Volume
{
  TransferFunction1D(CyclesGlobalState *s);
  virtual ~TransferFunction1D() override;

  void commitParameters() override;
  void finalize() override;
  bool isValid() const override;

  //std::unique_ptr<ccl::Geometry> makeCyclesGeometry() override;
  //ccl::Geometry* createCyclesGeometryNode() override;
  //void syncCyclesNode(ccl::Geometry* node) const override;

  ccl::Geometry* cyclesGeometry() const override;

  //box3 bounds() const override;

 private:
  //helium::IntrusivePtr<SpatialField> m_field;

  //box3 m_bounds;

  //helium::box1 m_valueRange{0.f, 1.f};
  //float m_densityScale{1.f};

  //helium::IntrusivePtr<Array1D> m_colorData;
  //helium::IntrusivePtr<Array1D> m_opacityData;

  //std::vector<anari_vec::float4> m_rgbaMap;

	helium::ChangeObserverPtr<Array1D> m_color;
	helium::ChangeObserverPtr<Array1D> m_opacity;

	box1 m_valueRange{ 0.f, 1.f };
	float m_unitDistance{ 1.f };
	anari_vec::vec4 m_uniformColor{ 1.f };
	float m_uniformOpacity{ 1.f };

	helium::ChangeObserverPtr<SpatialField> m_field;

	//std::vector<anari_vec::vec4> m_tf;
	//int m_tfDim{ 256 };

  // Cycles
  ccl::Shader *m_shader{nullptr};
  //ccl::ShaderGraph *m_graph{nullptr};

  // Nodes
  ccl::AttributeNode *m_attributeNode{nullptr};
  ccl::MapRangeNode *m_mapRangeNode{nullptr};
  ccl::RGBRampNode *m_rgbRampNode{nullptr};
  ccl::MathNode *m_mathNode{nullptr};

  ccl::PrincipledVolumeNode *m_volumeNode{nullptr};

  ccl::Shader *cyclesShader();

  void cleanupCyclesNode();

  ccl::Geometry* m_cyclesGeometryNode{ nullptr };
  bool m_fieldHandleChanged{ false };
};

} // namespace anari_cycles

CYCLES_ANARI_TYPEFOR_SPECIALIZATION(anari_cycles::Volume *, ANARI_VOLUME);
