#pragma once

#include <vector>

#include "gpupixel/filter/filter.h"
#include "gpupixel/gpupixel_define.h"

namespace gpupixel {

class GPUPIXEL_API FaceMaskFilter : public Filter {
 public:
  static std::shared_ptr<FaceMaskFilter> Create();
  ~FaceMaskFilter();

  bool Init();
  bool DoRender(bool updateSinks = true) override;

  void SetFaceLandmarks(const std::vector<float>& landmarks);
  bool HasValidMask() const { return has_face_; }

 private:
  FaceMaskFilter();

  static const std::vector<uint32_t>& FaceIndices();

  std::vector<float> face_vertices_;
  bool has_face_ = false;
};

}  // namespace gpupixel

