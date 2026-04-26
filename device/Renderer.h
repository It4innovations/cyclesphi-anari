// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Object.h"
#include "Array.h"

namespace anari_cycles {

	struct Renderer : public Object
	{
		Renderer(CyclesGlobalState* s, float defaultAmbientRadiance);
		~Renderer() override;

		void commitParameters() override;

		void makeRendererCurrent() const;

	public: 
		int spp() const;
		bool checkerboarding() const;
		bool denoise() const;
		int sampleLimit() const;
		//bool runAsync() const;

		static Renderer* createInstance(
			std::string_view subtype, CyclesGlobalState* d);

	private:
#if 0
		anari_vec::float4 m_backgroundColor;
		anari_vec::float3 m_ambientColor;
		float m_ambientIntensity;
		bool m_runAsync{ false };
#endif

#if 0
		private:
		struct {
			int background : 1;
			int ambientLight : 1;
			int denoise : 1;
		} m_needsUpdateStatus = {true, true, true};

		math::float4 m_backgroundColor;
		math::float3 m_ambientColor;
		float m_ambientIntensity;
		bool m_runAsync{false};
		bool m_denoise{false};
#endif

		anari_vec::vec4 m_bgColor{ 0.f, 0.f, 0.f, 1.f };
		int m_spp{ 1 };
		int m_maxRayDepth{ 0 };
		anari_vec::vec3 m_ambientColor{ 1.f };
		float m_ambientIntensity{ 0.f };
		float m_occlusionDistance{ 1e20f };
		bool m_checkerboard{ false };
		bool m_denoise{ false };
		int m_sampleLimit{ 0 };
		bool m_cullTriangleBF{ false };
		float m_volumeSamplingRate{ 1.f };

		helium::ChangeObserverPtr<Array2D> m_backgroundImage;

	private:
		void cleanup();
		float m_defaultAmbientRadiance{ 0.f };
	};

} // namespace anari_cycles

CYCLES_ANARI_TYPEFOR_SPECIALIZATION(anari_cycles::Renderer *, ANARI_RENDERER);
