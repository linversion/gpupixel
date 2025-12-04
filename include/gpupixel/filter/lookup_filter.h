#pragma once

#include <memory>
#include <string>

#include "gpupixel/filter/filter.h"

namespace gpupixel {

class SourceImage;

class GPUPIXEL_API LookupFilter : public Filter {
 public:
  static std::shared_ptr<LookupFilter> Create();

  ~LookupFilter();

  void SetIntensity(float intensity);
  void SetLookupImage(const std::string& path);
  void SetLookupImage(std::shared_ptr<SourceImage> image);

 protected:
  LookupFilter();
  bool Init();
  bool DoRender(bool update_sinks) override;

 private:
  float intensity_;
  std::shared_ptr<SourceImage> lookup_image_;
  std::string lookup_path_;
};

}  // namespace gpupixel

