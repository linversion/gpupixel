/*
 * GPUPixel
 *
 * Created by PixPark on 2021/6/24.
 * Copyright © 2021 PixPark. All rights reserved.
 */

#include "gpupixel/filter/beauty_face_filter.h"
#include "core/gpupixel_context.h"
namespace gpupixel {

BeautyFaceFilter::BeautyFaceFilter() {}

BeautyFaceFilter::~BeautyFaceFilter() {}

std::shared_ptr<BeautyFaceFilter> BeautyFaceFilter::Create() {
  auto ret = std::shared_ptr<BeautyFaceFilter>(new BeautyFaceFilter());
  gpupixel::GPUPixelContext::GetInstance()->SyncRunWithContext([&] {
    if (ret && !ret->Init()) {
      ret.reset();
    }
  });
  return ret;
}

bool BeautyFaceFilter::Init() {
  if (!FilterGroup::Init()) {
    return false;
  }

  box_blur_filter_ = BoxBlurFilter::Create();
  AddFilter(box_blur_filter_);

  box_high_pass_filter_ = BoxHighPassFilter::Create();
  AddFilter(box_high_pass_filter_);

  face_mask_filter_ = FaceMaskFilter::Create();
  AddFilter(face_mask_filter_);

  beauty_face_filter_ = BeautyFaceUnitFilter::Create();
  AddFilter(beauty_face_filter_);

  box_blur_filter_->AddSink(beauty_face_filter_, 1);
  box_high_pass_filter_->AddSink(beauty_face_filter_, 2);
  face_mask_filter_->AddSink(beauty_face_filter_, 3);

  SetTerminalFilter(beauty_face_filter_);

  box_blur_filter_->SetTexelSpacingMultiplier(4);
  SetRadius(4);

  RegisterProperty("whiteness", 0,
                   "The whiteness of filter with range between -1 and 1.",
                   [this](float& val) { SetWhite(val); });

  RegisterProperty("skin_smoothing", 0,
                   "The smoothing of filter with range between -1 and 1.",
                   [this](float& val) { SetBlurAlpha(val); });

  std::vector<float> default_landmarks;
  RegisterProperty("face_landmark", default_landmarks,
                   "Normalized [0,1] landmark coordinates used for whitening mask.",
                   [this](std::vector<float>& val) { SetFaceLandmarks(val); });
  return true;
}

void BeautyFaceFilter::SetInputFramebuffer(
    std::shared_ptr<GPUPixelFramebuffer> framebuffer,
    RotationMode rotation_mode /* = NoRotation*/,
    int texIdx /* = 0*/) {
  for (auto& filter : filters_) {
    filter->SetInputFramebuffer(framebuffer, rotation_mode, texIdx);
  }
}

void BeautyFaceFilter::SetHighPassDelta(float highPassDelta) {
  box_high_pass_filter_->SetDelta(highPassDelta);
}

void BeautyFaceFilter::SetSharpen(float sharpen) {
  beauty_face_filter_->SetSharpen(sharpen);
}

void BeautyFaceFilter::SetBlurAlpha(float blurAlpha) {
  beauty_face_filter_->SetBlurAlpha(blurAlpha);
}

void BeautyFaceFilter::SetWhite(float white) {
  beauty_face_filter_->SetWhite(white);
}

void BeautyFaceFilter::SetFaceLandmarks(const std::vector<float>& landmarks) {
  if (face_mask_filter_) {
    face_mask_filter_->SetFaceLandmarks(landmarks);
  }
  if (beauty_face_filter_) {
    bool enable_mask =
        face_mask_filter_ != nullptr && face_mask_filter_->HasValidMask();
    beauty_face_filter_->EnableFaceMask(enable_mask);
  }
}

void BeautyFaceFilter::SetRadius(float radius) {
  box_blur_filter_->SetRadius(radius);
  box_high_pass_filter_->SetRadius(radius);
}
}  // namespace gpupixel
