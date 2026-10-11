// Copyright 2025 Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Object.h"
#include "array/Array2D.h"
#include "sampler/Sampler.h"
// cycles
#include "scene/shader.h"
#include "scene/shader_graph.h"
// std
#include <memory>
#include <string>
#include <vector>

namespace anari_cycles {

// XmlShaderGraph /////////////////////////////////////////////////////////////
//
// A Cycles shader graph given as XML text by a parameter of an ANARI object:
// the 'cyclesXml' material (CYCLES_MATERIAL_CYCLES_XML), and the shaders of
// lights and of the world (CYCLES_SHADER_XML). The text is one <shader>
// element in the dialect of cyclesphi's XML reader (app/cycles_xml_bin.cpp).
//
// Images are 'image2D' sampler parameters of the owning object, of any name:
// an image_texture/environment_texture node whose 'filename' equals the name
// of such a parameter samples the array of that sampler. Only the array is
// used; filter and wrap modes come from the node.
//
// The owner keeps one of these per text parameter. It tells whether the text
// or an image it uses changed since the graph was last built, so that the
// owner only parses and hands a graph to Cycles when that is needed.

struct XmlShaderGraph
{
  // 'owner' has the sampler parameters and reports the status messages, which
  // start with 'what' (e.g. "light 'source'").
  XmlShaderGraph(Object *owner, std::string what);

  // False when 'source' and the images used by the graph are what build()
  // last saw: the graph it returned then (or its failure) still stands.
  bool changed(const std::string &source);

  // Read 'source'. The sockets of the <shader> element are set on 'settings'
  // and the images are bound. Returns nullptr when the text cannot be used
  // (malformed XML, unknown node type, nothing connected to the output: with
  // 'surfaceOnly' the Surface output must be connected, otherwise Volume or
  // Displacement will do), after reporting an ANARI error which ends with
  // 'fallback', what the owner does instead. Ignored values and links are
  // reported as one ANARI warning.
  std::unique_ptr<ccl::ShaderGraph> build(const std::string &source,
      ccl::Shader &settings,
      bool surfaceOnly,
      const char *fallback);

  // Whether the last build() returned a graph.
  bool valid() const;

  // Forget the built state and stop observing the images (the owner has no
  // text any more).
  void clear();

  size_t numImages() const;

  // Copy the shader sockets (displacement_method, emission_sampling_method,
  // ...) of 'settings' to 'shader'. Only values which differ are tagged as
  // modified on 'shader'; a default constructed 'settings' resets them.
  static void applySettings(ccl::Shader &shader, const ccl::Shader &settings);

 private:
  // An image name used by the graph and what it was resolved to when the
  // graph was built. The sampler and the array (if any) are observed, so
  // that a new image or new pixels finalize the owner again.
  struct ImageBinding
  {
    ImageBinding(const std::string &n, helium::BaseObject *observer)
        : name(n), sampler(observer), array(observer)
    {}

    std::string name;
    helium::ChangeObserverPtr<Sampler> sampler;
    helium::ChangeObserverPtr<Array2D> array; // null: not bound
    helium::TimeStamp dataStamp{0};
  };

  // The array of the image2D sampler in parameter 'name' of the owner,
  // nullptr when there is none.
  Array2D *imageArray(const std::string &name, Sampler **sampler) const;
  // True when an image of the built graph is no longer what it was bound to.
  bool imagesChanged();
  ImageBinding &imageBinding(const std::string &name);
  template <typename NODE_T>
  void bindImage(NODE_T *node);

  Object *m_owner{nullptr};
  std::string m_what;
  // State of the last build()
  std::string m_builtSource;
  std::vector<ImageBinding> m_images;
  bool m_built{false};
  bool m_valid{false};
};

} // namespace anari_cycles
