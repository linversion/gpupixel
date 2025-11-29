#include "gpupixel/filter/face_mask_filter.h"

#include "core/gpupixel_context.h"

namespace gpupixel {

namespace {

#if defined(GPUPIXEL_GLES_SHADER)
const std::string kFaceMaskVertexShader = R"(
    attribute vec2 position;

    void main() {
      gl_Position = vec4(position, 0.0, 1.0);
    })";

const std::string kFaceMaskFragmentShader = R"(
    precision mediump float;
    uniform float maskValue;

    void main() {
      gl_FragColor = vec4(maskValue, maskValue, maskValue, maskValue);
    })";
#elif defined(GPUPIXEL_GL_SHADER)
const std::string kFaceMaskVertexShader = R"(
    attribute vec2 position;

    void main() {
      gl_Position = vec4(position, 0.0, 1.0);
    })";

const std::string kFaceMaskFragmentShader = R"(
    uniform float maskValue;

    void main() {
      gl_FragColor = vec4(maskValue, maskValue, maskValue, maskValue);
    })";
#endif

}  // namespace

FaceMaskFilter::FaceMaskFilter() {}

FaceMaskFilter::~FaceMaskFilter() {}

std::shared_ptr<FaceMaskFilter> FaceMaskFilter::Create() {
  auto ret = std::shared_ptr<FaceMaskFilter>(new FaceMaskFilter());
  gpupixel::GPUPixelContext::GetInstance()->SyncRunWithContext([&] {
    if (ret && !ret->Init()) {
      ret.reset();
    }
  });
  return ret;
}

bool FaceMaskFilter::Init() {
  if (!Filter::InitWithShaderString(kFaceMaskVertexShader,
                                    kFaceMaskFragmentShader)) {
    return false;
  }
  return true;
}

bool FaceMaskFilter::DoRender(bool updateSinks) {
  GPUPixelContext::GetInstance()->SetActiveGlProgram(filter_program_);
  framebuffer_->Activate();
  GL_CALL(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
  GL_CALL(glClear(GL_COLOR_BUFFER_BIT));

  filter_program_->SetUniformValue("maskValue", has_face_ ? 1.0f : 0.0f);

  if (has_face_ && !face_vertices_.empty()) {
    GL_CALL(glVertexAttribPointer(filter_position_attribute_, 2, GL_FLOAT, 0, 0,
                                  face_vertices_.data()));
    const auto& face_indices = FaceIndices();
    GL_CALL(glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(face_indices.size()),
                           GL_UNSIGNED_INT, face_indices.data()));
  }

  framebuffer_->Deactivate();

  return Source::DoRender(updateSinks);
}

void FaceMaskFilter::SetFaceLandmarks(const std::vector<float>& landmarks) {
  if (landmarks.empty()) {
    face_vertices_.clear();
    has_face_ = false;
    return;
  }

  face_vertices_.resize(landmarks.size());
  for (size_t i = 0; i < landmarks.size(); ++i) {
    face_vertices_[i] = 2.0f * landmarks[i] - 1.0f;
  }
  has_face_ = true;
}

const std::vector<uint32_t>& FaceMaskFilter::FaceIndices() {
  static const std::vector<uint32_t> kFaceIndices = {
      33, 34, 64, 64, 34, 65, 65, 34, 107, 107, 34, 35, 35, 36, 107, 107, 36,
      66, 66, 107, 65, 66, 36, 67, 67, 36, 37, 37, 67, 43, 43, 38, 68, 68, 38,
      39, 39, 68, 69, 39, 40, 108, 39, 108, 69, 69, 108, 70, 70, 108, 41, 41,
      108, 40, 41, 70, 71, 71, 41, 42, 0, 33, 52, 33, 52, 64, 52, 64, 53, 64,
      53, 65, 65, 53, 72, 65, 72, 66, 66, 72, 54, 66, 54, 67, 54, 67, 55, 67,
      55, 78, 67, 78, 43, 52, 53, 57, 53, 72, 74, 53, 74, 57, 74, 57, 73, 72,
      54, 104, 72, 104, 74, 74, 104, 73, 73, 104, 56, 104, 56, 54, 54, 56, 55,
      68, 43, 79, 68, 79, 58, 68, 58, 59, 68, 59, 69, 69, 59, 75, 69, 75, 70,
      70, 75, 60, 70, 60, 71, 71, 60, 61, 71, 61, 42, 42, 61, 32, 61, 60, 62,
      60, 75, 77, 60, 77, 62, 77, 62, 76, 75, 77, 105, 77, 105, 76, 105, 76, 63,
      105, 63, 59, 105, 59, 75, 59, 63, 58, 0, 52, 1, 1, 52, 2, 2, 52, 57, 2,
      57, 3, 3, 57, 4, 4, 57, 109, 57, 109, 74, 74, 109, 56, 56, 109, 80, 80,
      109, 82, 82, 109, 7, 7, 109, 6, 6, 109, 5, 5, 109, 4, 56, 80, 55, 55, 80,
      78, 32, 61, 31, 31, 61, 30, 30, 61, 62, 30, 62, 29, 29, 62, 28, 28, 62,
      110, 62, 110, 76, 76, 110, 63, 63, 110, 81, 81, 110, 83, 83, 110, 25, 25,
      110, 26, 26, 110, 27, 27, 110, 28, 63, 81, 58, 58, 81, 79, 78, 43, 44, 43,
      44, 79, 78, 44, 80, 79, 81, 44, 80, 44, 45, 44, 81, 45, 80, 45, 46, 45,
      81, 46, 80, 46, 82, 81, 46, 83, 82, 46, 47, 47, 46, 48, 48, 46, 49, 49, 46,
      50, 50, 46, 51, 51, 46, 83, 7, 82, 84, 82, 84, 47, 84, 47, 85, 85, 47, 48,
      48, 85, 86, 86, 48, 49, 49, 86, 87, 49, 87, 88, 88, 49, 50, 88, 50, 89, 89,
      50, 51, 89, 51, 90, 51, 90, 83, 83, 90, 25, 84, 85, 96, 96, 85, 97, 97, 85,
      86, 86, 97, 98, 86, 98, 87, 87, 98, 88, 88, 98, 99, 88, 99, 89, 89, 99,
      100, 89, 100, 90, 90, 100, 91, 100, 91, 101, 101, 91, 92, 101, 92, 102,
      102, 92, 93, 102, 93, 94, 102, 94, 103, 103, 94, 95, 103, 95, 96, 96, 95,
      84, 96, 97, 103, 97, 103, 106, 97, 106, 98, 106, 103, 102, 106, 102, 101,
      106, 101, 99, 106, 98, 99, 99, 101, 100, 7, 84, 8, 8, 84, 9, 9, 84, 10, 10,
      84, 95, 10, 95, 11, 11, 95, 12, 12, 95, 94, 12, 94, 13, 13, 94, 14, 14, 94,
      93, 14, 93, 15, 15, 93, 16, 16, 93, 17, 17, 93, 18, 18, 93, 92, 18, 92, 19,
      19, 92, 20, 20, 92, 91, 20, 91, 21, 21, 91, 22, 22, 91, 90, 22, 90, 23, 23,
      90, 24, 24, 90, 25};
  return kFaceIndices;
}

}  // namespace gpupixel

