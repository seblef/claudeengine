#pragma once

#include <algorithm>

#include "core/Vec2f.h"

namespace core {

// Axis-aligned 2D rectangle stored as (x, y, w, h) in screen space.
//
// Convention: origin is the top-left corner, axes grow right (+x) and down
// (+y). A valid rect has w >= 0 and h >= 0. Use IsValid() to check after
// Intersection() — an empty intersection returns a rect with negative extents.
class Rect {
 public:
  // ---- Constants (defined after class — class must be complete) ------------

  static const Rect kZero;  // (0, 0, 0, 0)
  static const Rect kUnit;  // (0, 0, 1, 1)

  // ---- Construction --------------------------------------------------------

  Rect() = default;
  Rect(const Rect&) = default;
  Rect& operator=(const Rect&) = default;

  inline Rect(float x, float y, float w, float h) : x_(x), y_(y), w_(w), h_(h) {}
  inline Rect(const Vec2f& position, const Vec2f& size)
      : x_(position.x), y_(position.y), w_(size.x), h_(size.y) {}

  // Constructs from two corners; argument order does not matter.
  [[nodiscard]] static inline Rect FromCorners(const Vec2f& a, const Vec2f& b) {
    float x = std::min(a.x, b.x);
    float y = std::min(a.y, b.y);
    return {x, y, std::max(a.x, b.x) - x, std::max(a.y, b.y) - y};
  }

  // Constructs a rect centered at center with the given size.
  [[nodiscard]] static inline Rect FromCenter(const Vec2f& center,
                                              const Vec2f& size) {
    return {center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x,
            size.y};
  }

  // ---- Getters / Setters ---------------------------------------------------

  [[nodiscard]] inline float  GetX()      const { return x_; }
  [[nodiscard]] inline float  GetY()      const { return y_; }
  [[nodiscard]] inline float  GetWidth()  const { return w_; }
  [[nodiscard]] inline float  GetHeight() const { return h_; }

  [[nodiscard]] inline Vec2f  GetPosition()    const { return {x_, y_}; }
  [[nodiscard]] inline Vec2f  GetSize()        const { return {w_, h_}; }
  [[nodiscard]] inline Vec2f  GetCenter()      const { return {x_ + w_ * 0.5f, y_ + h_ * 0.5f}; }
  [[nodiscard]] inline Vec2f  GetTopLeft()     const { return {x_, y_}; }
  [[nodiscard]] inline Vec2f  GetTopRight()    const { return {x_ + w_, y_}; }
  [[nodiscard]] inline Vec2f  GetBottomLeft()  const { return {x_, y_ + h_}; }
  [[nodiscard]] inline Vec2f  GetBottomRight() const { return {x_ + w_, y_ + h_}; }

  inline void SetPosition(float x, float y) { x_ = x; y_ = y; }
  inline void SetPosition(const Vec2f& pos)  { x_ = pos.x; y_ = pos.y; }
  inline void SetSize(float w, float h)      { w_ = w; h_ = h; }
  inline void SetSize(const Vec2f& size)     { w_ = size.x; h_ = size.y; }

  // ---- Validation ----------------------------------------------------------

  // Returns true when width and height are both non-negative.
  [[nodiscard]] inline bool IsValid() const { return w_ >= 0.f && h_ >= 0.f; }

  // ---- Arithmetic operators ------------------------------------------------

  // Translation by a pixel offset.
  [[nodiscard]] inline Rect operator+(const Vec2f& v) const { return {x_ + v.x, y_ + v.y, w_, h_}; }
  [[nodiscard]] inline Rect operator-(const Vec2f& v) const { return {x_ - v.x, y_ - v.y, w_, h_}; }
  inline Rect& operator+=(const Vec2f& v) { x_ += v.x; y_ += v.y; return *this; }
  inline Rect& operator-=(const Vec2f& v) { x_ -= v.x; y_ -= v.y; return *this; }

  // Uniform scale applied to both position and size.
  [[nodiscard]] inline Rect operator*(float s) const { return {x_ * s, y_ * s, w_ * s, h_ * s}; }
  [[nodiscard]] inline Rect operator/(float s) const { return {x_ / s, y_ / s, w_ / s, h_ / s}; }
  inline Rect& operator*=(float s) { x_ *= s; y_ *= s; w_ *= s; h_ *= s; return *this; }
  inline Rect& operator/=(float s) { x_ /= s; y_ /= s; w_ /= s; h_ /= s; return *this; }

  [[nodiscard]] inline bool operator==(const Rect& rhs) const {
    return x_ == rhs.x_ && y_ == rhs.y_ && w_ == rhs.w_ && h_ == rhs.h_;
  }
  [[nodiscard]] inline bool operator!=(const Rect& rhs) const { return !(*this == rhs); }

  // ---- Spatial queries -----------------------------------------------------

  // Returns true when point lies inside (inclusive boundary).
  [[nodiscard]] inline bool Contains(const Vec2f& point) const {
    return point.x >= x_ && point.x <= x_ + w_ &&
           point.y >= y_ && point.y <= y_ + h_;
  }

  // Returns true when other is fully inside this rect (inclusive boundary).
  [[nodiscard]] inline bool Contains(const Rect& other) const {
    return other.x_ >= x_ && other.y_ >= y_ &&
           other.x_ + other.w_ <= x_ + w_ &&
           other.y_ + other.h_ <= y_ + h_;
  }

  // Returns true when the two rects share any area (touching counts).
  [[nodiscard]] inline bool Overlaps(const Rect& other) const {
    return x_ <= other.x_ + other.w_ && x_ + w_ >= other.x_ &&
           y_ <= other.y_ + other.h_ && y_ + h_ >= other.y_;
  }

  // ---- Geometry operations -------------------------------------------------

  // Returns a rect expanded (positive) or contracted (negative) by amount on
  // all four sides. Center is preserved; size changes by 2 * amount per axis.
  [[nodiscard]] inline Rect Expanded(float amount) const {
    return {x_ - amount, y_ - amount, w_ + 2.f * amount, h_ + 2.f * amount};
  }

  // Per-axis variant: dx expands left/right, dy expands top/bottom.
  [[nodiscard]] inline Rect Expanded(float dx, float dy) const {
    return {x_ - dx, y_ - dy, w_ + 2.f * dx, h_ + 2.f * dy};
  }

  // Returns the largest rect contained in both this and other.
  // Check IsValid() on the result — it will be invalid when there is no overlap.
  [[nodiscard]] inline Rect Intersection(const Rect& other) const {
    float nx = std::max(x_, other.x_);
    float ny = std::max(y_, other.y_);
    return {nx, ny,
            std::min(x_ + w_, other.x_ + other.w_) - nx,
            std::min(y_ + h_, other.y_ + other.h_) - ny};
  }

  // Returns the smallest rect enclosing both this and other.
  [[nodiscard]] inline Rect Union(const Rect& other) const {
    float nx = std::min(x_, other.x_);
    float ny = std::min(y_, other.y_);
    return {nx, ny,
            std::max(x_ + w_, other.x_ + other.w_) - nx,
            std::max(y_ + h_, other.y_ + other.h_) - ny};
  }

  // Returns this rect clamped so it fits within bounds.
  [[nodiscard]] inline Rect Clamped(const Rect& bounds) const {
    float cx = std::max(x_, bounds.x_);
    float cy = std::max(y_, bounds.y_);
    float cx2 = std::min(x_ + w_, bounds.x_ + bounds.w_);
    float cy2 = std::min(y_ + h_, bounds.y_ + bounds.h_);
    return {cx, cy, cx2 - cx, cy2 - cy};
  }

 private:
  float x_ = 0.f;
  float y_ = 0.f;
  float w_ = 0.f;
  float h_ = 0.f;
};

// Constant definitions — class must be complete before these initialise.
inline const Rect Rect::kZero{0.f, 0.f, 0.f, 0.f};
inline const Rect Rect::kUnit{0.f, 0.f, 1.f, 1.f};

[[nodiscard]] inline Rect operator*(float s, const Rect& r) { return r * s; }

}  // namespace core
