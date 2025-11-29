/*
 * GPUPixel
 *
 * Created by PixPark on 2021/6/24.
 * Copyright © 2021 PixPark. All rights reserved.
 */

#include "gpupixel/sink/sink_render.h"
#include <cstring>
#include "core/gpupixel_context.h"
#include "gpupixel/filter/filter.h"
#include "utils/util.h"

namespace gpupixel {

std::shared_ptr<SinkRender> SinkRender::Create() {
  auto ret = std::shared_ptr<SinkRender>(new SinkRender());
  gpupixel::GPUPixelContext::GetInstance()->SyncRunWithContext(
      [&] { ret->Init(); });
  return ret;
}

SinkRender::SinkRender()
    : view_width_(0),
      view_height_(0),
      fill_mode_(FillMode::PreserveAspectRatio),
      display_program_(0),
      position_attribute_location_(0),
      tex_coord_attribute_location_(0),
      color_map_uniform_location_(0) {
  background_color_.r = 0.0;
  background_color_.g = 0.0;
  background_color_.b = 0.0;
  background_color_.a = 0.0;

  const float default_vertices[] = {
      -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f, 1.0f,  1.0f,
  };
  std::memcpy(display_vertices_, default_vertices, sizeof(display_vertices_));
  const float* default_tex = GetTextureCoordinate(NoRotation);
  std::memcpy(texture_coordinates_, default_tex, sizeof(texture_coordinates_));
  cached_tex_rotation_ = NoRotation;
  texture_cache_initialized_ = true;
}

SinkRender::~SinkRender() {
  if (display_program_) {
    delete display_program_;
    display_program_ = 0;
  }
  DestroyVertexBuffers();
}

void SinkRender::Init() {
  display_program_ = GPUPixelGLProgram::CreateWithShaderString(
      kDefaultVertexShader, kDefaultFragmentShader);
  position_attribute_location_ =
      display_program_->GetAttribLocation("position");
  tex_coord_attribute_location_ =
      display_program_->GetAttribLocation("inputTextureCoordinate");
  color_map_uniform_location_ =
      display_program_->GetUniformLocation("textureCoordinate");
  GPUPixelContext::GetInstance()->SetActiveGlProgram(display_program_);
  GL_CALL(glEnableVertexAttribArray(position_attribute_location_));
  GL_CALL(glEnableVertexAttribArray(tex_coord_attribute_location_));
  InitVertexBuffers();
};

void SinkRender::SetInputFramebuffer(
    std::shared_ptr<GPUPixelFramebuffer> framebuffer,
    RotationMode rotation_mode /* = NoRotation*/,
    int tex_idx /* = 0*/) {
  std::shared_ptr<GPUPixelFramebuffer> last_input_framebuffer;
  RotationMode last_input_rotation = NoRotation;
  if (input_framebuffers_.find(0) != input_framebuffers_.end()) {
    last_input_framebuffer = input_framebuffers_[0].frame_buffer;
    last_input_rotation = input_framebuffers_[0].rotation_mode;
  }

  Sink::SetInputFramebuffer(framebuffer, rotation_mode, tex_idx);

  if (last_input_framebuffer != framebuffer && framebuffer &&
      (!last_input_framebuffer ||
       !(last_input_framebuffer->GetWidth() == framebuffer->GetWidth() &&
         last_input_framebuffer->GetHeight() == framebuffer->GetHeight() &&
         last_input_rotation == rotation_mode))) {
    UpdateDisplayVertices();
  }

  if (framebuffer) {
    UpdateTextureCoordinatesCache(rotation_mode);
  }
}

void SinkRender::SetFillMode(FillMode fill_mode) {
  if (fill_mode_ != fill_mode) {
    fill_mode_ = fill_mode;
    UpdateDisplayVertices();
  }
}

void SinkRender::SetMirror(bool mirror) {
  if (mirror_ != mirror) {
    mirror_ = mirror;
    texture_cache_initialized_ = false;
    if (input_framebuffers_.find(0) != input_framebuffers_.end()) {
      UpdateTextureCoordinatesCache(input_framebuffers_[0].rotation_mode);
    }
  }
}

void SinkRender::SetRenderSize(int width, int height) {
  if (view_width_ != width || view_height_ != height) {
    view_width_ = width;
    view_height_ = height;
    UpdateDisplayVertices();
  }
}

void SinkRender::Render() {
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));

  if (view_width_ == 0 || view_height_ == 0) {
    LOG_WARN("SinkRender: view_width_ or view_height_ is 0");
    return;
  }
  GL_CALL(glViewport(0, 0, view_width_, view_height_));
  GL_CALL(glClearColor(background_color_.r, background_color_.g,
                       background_color_.b, background_color_.a));
  GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
  GPUPixelContext::GetInstance()->SetActiveGlProgram(display_program_);
  GL_CALL(glActiveTexture(GL_TEXTURE0));
  GL_CALL(glBindTexture(GL_TEXTURE_2D,
                        input_framebuffers_[0].frame_buffer->GetTexture()));
  GL_CALL(glUniform1i(color_map_uniform_location_, 0));
  UpdateTextureCoordinatesCache(input_framebuffers_[0].rotation_mode);
  if (vertex_buffer_dirty_) {
    UploadVertexBuffer();
  }
  if (tex_coord_buffer_dirty_) {
    UploadTexCoordBuffer();
  }

  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id_));
  GL_CALL(glVertexAttribPointer(position_attribute_location_, 2, GL_FLOAT,
                                GL_FALSE, 0, 0));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, tex_coord_buffer_id_));
  GL_CALL(glVertexAttribPointer(tex_coord_attribute_location_, 2, GL_FLOAT,
                                GL_FALSE, 0, 0));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));

  GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
}

void SinkRender::UpdateDisplayVertices() {
  if (input_framebuffers_.find(0) == input_framebuffers_.end() ||
      input_framebuffers_[0].frame_buffer == 0) {
    return;
  }

  std::shared_ptr<GPUPixelFramebuffer> input_framebuffer =
      input_framebuffers_[0].frame_buffer;
  RotationMode input_rotation = input_framebuffers_[0].rotation_mode;

  int rotated_framebuffer_width = input_framebuffer->GetWidth();
  int rotated_framebuffer_height = input_framebuffer->GetHeight();
  if (rotationSwapsSize(input_rotation)) {
    rotated_framebuffer_width = input_framebuffer->GetHeight();
    rotated_framebuffer_height = input_framebuffer->GetWidth();
  }

  float framebuffer_aspect_ratio =
      rotated_framebuffer_height / (float)rotated_framebuffer_width;
  float view_aspect_ratio = view_height_ / (float)view_width_;

  float inset_framebuffer_width = 0.0;
  float inset_framebuffer_height = 0.0;
  if (framebuffer_aspect_ratio > view_aspect_ratio) {
    inset_framebuffer_width = view_height_ / (float)rotated_framebuffer_height *
                              rotated_framebuffer_width;
    inset_framebuffer_height = view_height_;
  } else {
    inset_framebuffer_width = view_width_;
    inset_framebuffer_height = view_width_ / (float)rotated_framebuffer_width *
                               rotated_framebuffer_height;
  }

  float scaled_width = 1.0;
  float scaled_height = 1.0;
  if (fill_mode_ == FillMode::PreserveAspectRatio) {
    scaled_width = inset_framebuffer_width / view_width_;
    scaled_height = inset_framebuffer_height / view_height_;
  } else if (fill_mode_ == FillMode::PreserveAspectRatioAndFill) {
    scaled_width = view_width_ / inset_framebuffer_height;
    scaled_height = view_height_ / inset_framebuffer_width;
  }

  display_vertices_[0] = -scaled_width;
  display_vertices_[1] = -scaled_height;
  display_vertices_[2] = scaled_width;
  display_vertices_[3] = -scaled_height;
  display_vertices_[4] = -scaled_width;
  display_vertices_[5] = scaled_height;
  display_vertices_[6] = scaled_width;
  display_vertices_[7] = scaled_height;
  vertex_buffer_dirty_ = true;
}

const float* SinkRender::GetTextureCoordinate(RotationMode rotation_mode) {
  static const float no_rotation_texture_coordinates[] = {
      0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
  };

  static const float rotate_right_texture_coordinates[] = {
      1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
  };

  static const float rotate_left_texture_coordinates[] = {
      0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
  };

  static const float vertical_flip_texture_coordinates[] = {
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
  };

  static const float horizontal_flip_texture_coordinates[] = {
      1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
  };

  static const float rotate_right_vertical_flip_texture_coordinates[] = {
      1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  };

  static const float rotate_right_horizontal_flip_texture_coordinates[] = {
      0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f,
  };

  static const float rotate_180_texture_coordinates[] = {
      1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
  };

  switch (rotation_mode) {
    case NoRotation: {
      if (mirror_) {
        return horizontal_flip_texture_coordinates;
      } else {
        return no_rotation_texture_coordinates;
      }
    }
    case RotateLeft:
      return rotate_left_texture_coordinates;
    case RotateRight:
      return rotate_right_texture_coordinates;
    case FlipVertical:
      return vertical_flip_texture_coordinates;
    case FlipHorizontal:
      return horizontal_flip_texture_coordinates;
    case RotateRightFlipVertical:
      return rotate_right_vertical_flip_texture_coordinates;
    case RotateRightFlipHorizontal:
      return rotate_right_horizontal_flip_texture_coordinates;
    case Rotate180:
      return rotate_180_texture_coordinates;
  }
}

void SinkRender::UpdateTextureCoordinatesCache(RotationMode rotation_mode) {
  if (!texture_cache_initialized_ || rotation_mode != cached_tex_rotation_ ||
      cached_tex_mirror_ != mirror_) {
    const float* coords = GetTextureCoordinate(rotation_mode);
    std::memcpy(texture_coordinates_, coords, sizeof(texture_coordinates_));
    cached_tex_rotation_ = rotation_mode;
    cached_tex_mirror_ = mirror_;
    texture_cache_initialized_ = true;
    tex_coord_buffer_dirty_ = true;
  }
}

void SinkRender::InitVertexBuffers() {
  if (!vertex_buffer_id_) {
    GL_CALL(glGenBuffers(1, &vertex_buffer_id_));
    vertex_buffer_dirty_ = true;
  }
  if (!tex_coord_buffer_id_) {
    GL_CALL(glGenBuffers(1, &tex_coord_buffer_id_));
    tex_coord_buffer_dirty_ = true;
  }
  UploadVertexBuffer();
  UploadTexCoordBuffer();
}

void SinkRender::DestroyVertexBuffers() {
  if (!vertex_buffer_id_ && !tex_coord_buffer_id_) {
    return;
  }
  GPUPixelContext::GetInstance()->SyncRunWithContext([&] {
    if (vertex_buffer_id_) {
      GL_CALL(glDeleteBuffers(1, &vertex_buffer_id_));
      vertex_buffer_id_ = 0;
    }
    if (tex_coord_buffer_id_) {
      GL_CALL(glDeleteBuffers(1, &tex_coord_buffer_id_));
      tex_coord_buffer_id_ = 0;
    }
  });
}

void SinkRender::UploadVertexBuffer() {
  if (!vertex_buffer_id_) {
    return;
  }
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id_));
  GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(display_vertices_),
                       display_vertices_, GL_DYNAMIC_DRAW));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
  vertex_buffer_dirty_ = false;
}

void SinkRender::UploadTexCoordBuffer() {
  if (!tex_coord_buffer_id_) {
    return;
  }
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, tex_coord_buffer_id_));
  GL_CALL(glBufferData(GL_ARRAY_BUFFER, sizeof(texture_coordinates_),
                       texture_coordinates_, GL_DYNAMIC_DRAW));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
  tex_coord_buffer_dirty_ = false;
}

}  // namespace gpupixel
