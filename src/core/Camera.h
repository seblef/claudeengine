#pragma once

#include "core/CoordinateSystem.h"
#include "core/Mat4f.h"
#include "core/ProjectionType.h"
#include "core/Vec2f.h"
#include "core/Vec3f.h"

namespace core {

// Camera encapsulating view and projection transforms.
//
// Matrices are NOT rebuilt automatically when properties change; call
// UpdateMatrices() explicitly after modifying any combination of properties.
//
// screen_center stores (half_width, half_height) of the viewport:
//   - perspective:    aspect ratio    = screen_center.x / screen_center.y
//   - orthographic:   frustum extents = ±screen_center.x, ±screen_center.y
class Camera {
 public:
  Camera(ProjectionType proj_type, CoordinateSystem coord_system);

  // ---- Setters ---------------------------------------------------------------
  void SetPosition(Vec3f position);
  void SetTarget(Vec3f target);
  void SetMinDepth(float min_depth);
  void SetMaxDepth(float max_depth);
  void SetScreenCenter(Vec2f screen_center);
  void SetFOV(float fov_y);  // radians
  void SetUp(Vec3f up);

  // ---- Getters ---------------------------------------------------------------
  [[nodiscard]] Vec3f            GetPosition()         const { return position_;      }
  [[nodiscard]] Vec3f            GetTarget()           const { return target_;        }
  [[nodiscard]] float            GetMinDepth()         const { return min_depth_;     }
  [[nodiscard]] float            GetMaxDepth()         const { return max_depth_;     }
  [[nodiscard]] Vec2f            GetScreenCenter()     const { return screen_center_; }
  [[nodiscard]] float            GetFOV()              const { return fov_;           }
  [[nodiscard]] Vec3f            GetUp()               const { return up_;            }
  [[nodiscard]] ProjectionType   GetProjectionType()   const { return proj_type_;     }
  [[nodiscard]] CoordinateSystem GetCoordinateSystem() const { return coord_system_;  }

  // Recomputes proj_, view_, view_proj_, inv_view_, x_axis_, y_axis_.
  void UpdateMatrices();

  // ---- Matrix getters --------------------------------------------------------
  [[nodiscard]] const Mat4f& GetProjectionMatrix()     const { return proj_;      }
  [[nodiscard]] const Mat4f& GetViewMatrix()           const { return view_;      }
  [[nodiscard]] const Mat4f& GetViewProjectionMatrix() const { return view_proj_; }
  [[nodiscard]] const Mat4f& GetInverseViewMatrix()    const { return inv_view_;  }

  // View-space axes in world space: right (x) and up (y), set by UpdateMatrices().
  [[nodiscard]] Vec3f GetXAxis() const { return x_axis_; }
  [[nodiscard]] Vec3f GetYAxis() const { return y_axis_; }

 private:
  ProjectionType   proj_type_;
  CoordinateSystem coord_system_;

  Vec3f  position_      = {0.f, 0.f,  0.f};
  Vec3f  target_        = {0.f, 0.f, -1.f};
  float  min_depth_     = 0.1f;
  float  max_depth_     = 1000.f;
  Vec2f  screen_center_ = {0.f, 0.f};
  float  fov_           = 0.7853982f;  // pi/4 radians = 45 degrees
  Vec3f  up_            = Vec3f::kAxisY;

  Mat4f  proj_;
  Mat4f  view_;
  Mat4f  view_proj_;
  Mat4f  inv_view_;

  Vec3f  x_axis_;
  Vec3f  y_axis_;
};

}  // namespace core
