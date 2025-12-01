#include "gpupixel/filter/lookup_filter.h"

#include <algorithm>

#include "core/gpupixel_context.h"
#include "gpupixel/source/source_image.h"
#include "utils/logging.h"
#include "utils/util.h"

namespace gpupixel {

#if defined(GPUPIXEL_GLES_SHADER)
const std::string kLookupFragmentShaderString = R"(
    precision mediump float;
    varying vec2 textureCoordinate;
    uniform sampler2D inputImageTexture;
    uniform sampler2D lookupTexture;
    uniform float intensity;

    vec3 applyLookup(vec3 textureColor) {
      float blueColor = textureColor.b * 63.0;

      vec2 quad1;
      quad1.y = floor(floor(blueColor) / 8.0);
      quad1.x = floor(blueColor) - (quad1.y * 8.0);

      vec2 quad2;
      quad2.y = floor(ceil(blueColor) / 8.0);
      quad2.x = ceil(blueColor) - (quad2.y * 8.0);

      vec2 texPos1;
      texPos1.x = (quad1.x * 0.125) + 0.5 / 512.0 +
                  ((0.125 - 1.0 / 512.0) * textureColor.r);
      texPos1.y = (quad1.y * 0.125) + 0.5 / 512.0 +
                  ((0.125 - 1.0 / 512.0) * textureColor.g);

      vec2 texPos2;
      texPos2.x = (quad2.x * 0.125) + 0.5 / 512.0 +
                  ((0.125 - 1.0 / 512.0) * textureColor.r);
      texPos2.y = (quad2.y * 0.125) + 0.5 / 512.0 +
                  ((0.125 - 1.0 / 512.0) * textureColor.g);

      vec3 newColor1 = texture2D(lookupTexture, texPos1).rgb;
      vec3 newColor2 = texture2D(lookupTexture, texPos2).rgb;
      return mix(newColor1, newColor2, fract(blueColor));
    }

    void main() {
      vec4 textureColor = texture2D(inputImageTexture, textureCoordinate);
      vec3 lookupColor = applyLookup(textureColor.rgb);
      gl_FragColor = vec4(mix(textureColor.rgb, lookupColor, intensity), textureColor.a);
    })";
#elif defined(GPUPIXEL_GL_SHADER)
const std::string kLookupFragmentShaderString = R"(
    varying vec2 textureCoordinate;
    uniform sampler2D inputImageTexture;
    uniform sampler2D lookupTexture;
    uniform float intensity;

    vec3 applyLookup(vec3 textureColor) {
      float blueColor = textureColor.b * 63.0;

      vec2 quad1;
      quad1.y = floor(floor(blueColor) / 8.0);
      quad1.x = floor(blueColor) - (quad1.y * 8.0);

      vec2 quad2;
      quad2.y = floor(ceil(blueColor) / 8.0);
      quad2.x = ceil(blueColor) - (quad2.y * 8.0);

      vec2 texPos1;
      texPos1.x = (quad1.x * 0.125) + 0.5 / 512.0 +
                  ((0.125 - 1.0 / 512.0) * textureColor.r);
      texPos1.y = (quad1.y * 0.125) + 0.5 / 512.0 +
                  ((0.125 - 1.0 / 512.0) * textureColor.g);

      vec2 texPos2;
      texPos2.x = (quad2.x * 0.125) + 0.5 / 512.0 +
                  ((0.125 - 1.0 / 512.0) * textureColor.r);
      texPos2.y = (quad2.y * 0.125) + 0.5 / 512.0 +
                  ((0.125 - 1.0 / 512.0) * textureColor.g);

      vec3 newColor1 = texture2D(lookupTexture, texPos1).rgb;
      vec3 newColor2 = texture2D(lookupTexture, texPos2).rgb;
      return mix(newColor1, newColor2, fract(blueColor));
    }

    void main() {
      vec4 textureColor = texture2D(inputImageTexture, textureCoordinate);
      vec3 lookupColor = applyLookup(textureColor.rgb);
      gl_FragColor = vec4(mix(textureColor.rgb, lookupColor, intensity), textureColor.a);
    })";
#endif

LookupFilter::LookupFilter() : intensity_(1.0f) {}

LookupFilter::~LookupFilter() = default;

std::shared_ptr<LookupFilter> LookupFilter::Create() {
  auto ret = std::shared_ptr<LookupFilter>(new LookupFilter());
  gpupixel::GPUPixelContext::GetInstance()->SyncRunWithContext([&] {
    if (ret && !ret->Init()) {
      ret.reset();
    }
  });
  return ret;
}

bool LookupFilter::Init() {
  if (!InitWithFragmentShaderString(kLookupFragmentShaderString)) {
    return false;
  }

  RegisterProperty("lookup_intensity", intensity_,
                   "Blend level of LUT result between 0 and 1.",
                   [this](float& value) { SetIntensity(value); });

  RegisterProperty("lookup_path", std::string(""),
                   "Absolute or relative path to a LUT texture image.",
                   [this](std::string& value) { SetLookupImage(value); });

  auto default_path = Util::GetResourcePath() / "res" / "lookup_light.png";
  if (fs::exists(default_path)) {
    SetLookupImage(default_path.string());
  } else {
    LOG_WARN("LookupFilter: default LUT not found at {}", default_path.string());
  }

  return true;
}

void LookupFilter::SetIntensity(float intensity) {
  intensity_ = std::clamp(intensity, 0.0f, 1.0f);
}

void LookupFilter::SetLookupImage(std::shared_ptr<SourceImage> image) {
  lookup_image_ = image;
}

void LookupFilter::SetLookupImage(const std::string& path) {
  fs::path lookup_path(path);
  if (lookup_path.empty()) {
    lookup_path = Util::GetResourcePath() / "res" / "lookup_light.png";
  } else if (!lookup_path.is_absolute()) {
    auto candidate = Util::GetResourcePath() / lookup_path;
    if (fs::exists(candidate)) {
      lookup_path = candidate;
    } else {
      lookup_path = Util::GetResourcePath() / "res" / lookup_path;
    }
  }

  if (!fs::exists(lookup_path)) {
    LOG_ERROR("LookupFilter: lookup texture {} not found", lookup_path.string());
    return;
  }

  auto image = SourceImage::Create(lookup_path.string());
  if (image) {
    lookup_image_ = image;
    lookup_path_ = lookup_path.string();
    LOG_INFO("LookupFilter: loaded LUT {}", lookup_path_);
  }
}

bool LookupFilter::DoRender(bool update_sinks) {
  float blend = intensity_;
  if (!lookup_image_) {
    blend = 0.0f;
  } else {
    GL_CALL(glActiveTexture(GL_TEXTURE3));
    GL_CALL(glBindTexture(GL_TEXTURE_2D,
                          lookup_image_->GetFramebuffer()->GetTexture()));
    filter_program_->SetUniformValue("lookupTexture", 3);
  }

  filter_program_->SetUniformValue("intensity", blend);
  return Filter::DoRender(update_sinks);
}

}  // namespace gpupixel

