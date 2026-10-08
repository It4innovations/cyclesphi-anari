// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CyclesGlobalState.h"
// std
#include <string>

namespace ccl {
class Shader;
} // namespace ccl

namespace anari_cycles {

// cyclesphi XML scene support.
//
// The XML directory is taken from the device parameter 'cyclesphi.xmlPath',
// the CYCLES_XML_PATH environment variable, or <plugin dir>/cycles/anari when
// XML support is enabled with 'cyclesphi.xmlScene' / CYCLES_ANARI_XML_SCENE.
//
//  - cycles_default_scene.xml: film, integrator and default background shader
//    settings, loaded once at device initialization. Its background shader
//    nodes named "bgColor" and "ambientIntensity" are then driven by the
//    renderer 'background' and 'ambientRadiance' parameters.
//  - <subtype>.xml (matte, physicallyBased, transferFunction1D): shader graph
//    templates, used instead of the built-in graphs when enabled with
//    'cyclesphi.xmlTemplates' / CYCLES_ANARI_XML_TEMPLATES.

// Resolve the XML settings from the device parameters and the environment.
void xmlConfigure(CyclesGlobalState &state,
    const std::string &xmlPathParam,
    bool xmlSceneParam,
    bool xmlTemplatesParam);

// Load cycles_default_scene.xml into the scene, if XML support is enabled and
// the file exists.
void xmlLoadDefaultScene(CyclesGlobalState &state);

// Replace the graph of the shader with the template <name>.xml. Returns false
// when templates are disabled or there is no such template.
bool xmlApplyShaderTemplate(
    CyclesGlobalState &state, ccl::Shader *shader, const std::string &name);

} // namespace anari_cycles
