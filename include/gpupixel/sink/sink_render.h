/*
 * GPUPixel
 *
 * Created by PixPark on 2021/6/24.
 * Copyright © 2021 PixPark. All rights reserved.
 */

#pragma once

#include "gpupixel/sink/sink.h"

namespace gpupixel {
class GPUPixelGLProgram;
class GPUPIXEL_API SinkRender : public Sink {
 public:
  enum FillMode {
    Stretch = 0,  // Stretch to fill the view, and may distort the image
    PreserveAspectRatio = 1,  // preserve the aspect ratio of the image
    PreserveAspectRatioAndFill =
        2  // preserve the aspect ratio, and zoom in to fill the view
  };
  static std::shared_ptr<SinkRender> Create();

  ~SinkRender() override;

  void Init();
  void SetInputFramebuffer(std::shared_ptr<GPUPixelFramebuffer> framebuffer,
                           RotationMode rotation_mode = NoRotation,
                           int tex_idx = 0) override;
  void SetFillMode(FillMode fill_mode);
  void SetMirror(bool mirror);
  void SetRenderSize(int width, int height);
  void Render() override;

 private:
  SinkRender();
  int view_width_ = 0;
  int view_height_ = 0;
  FillMode fill_mode_;
  bool mirror_ = false;
  GPUPixelGLProgram* display_program_;
  uint32_t position_attribute_location_;
  uint32_t tex_coord_attribute_location_;
  uint32_t color_map_uniform_location_;
  struct {
    float r;
    float g;
    float b;
    float a;
  } background_color_;

  float display_vertices_[8];
  float texture_coordinates_[8];

  uint32_t vertex_buffer_id_ = 0;
  uint32_t tex_coord_buffer_id_ = 0;
  bool vertex_buffer_dirty_ = true;
  bool tex_coord_buffer_dirty_ = true;
  RotationMode cached_tex_rotation_ = NoRotation;
  bool cached_tex_mirror_ = false;
  bool texture_cache_initialized_ = false;

  void UpdateDisplayVertices();
  void UpdateTextureCoordinatesCache(RotationMode rotation_mode);
  void InitVertexBuffers();
  void DestroyVertexBuffers();
  void UploadVertexBuffer();
  void UploadTexCoordBuffer();
  const float* GetTextureCoordinate(RotationMode rotation_mode);
};

}  // namespace gpupixel
