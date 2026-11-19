#pragma once

#include <kinetra/view/camera.h>

namespace lab {

/// Moves a camera by `offset` world units for one frame's drawing and puts it back when
/// destroyed, also when drawing throws.
///
/// Demo and Dotio use it to center the view on the drawn (interpolated) player while the
/// session keeps the camera of the simulated one. `camera` is borrowed and must outlive
/// the shift.
class ScopedCameraShift {
  public:
    ScopedCameraShift(kinetra::Camera2D &camera, kinetra::Vec2 offset)
        : camera_(camera), saved_(camera.position) {
        camera_.position += offset;
    }
    ~ScopedCameraShift() { camera_.position = saved_; }
    ScopedCameraShift(const ScopedCameraShift &) = delete;
    ScopedCameraShift &operator=(const ScopedCameraShift &) = delete;

  private:
    kinetra::Camera2D &camera_;
    kinetra::Vec2 saved_;
};

} // namespace lab
