#include "editor/PhysicsGizmoRenderer.h"

#include "core/Color.h"
#include "physics/MotionType.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsBodyDesc.h"
#include "physics/PhysicsShapeDesc.h"
#include "physics/PhysicsShapeType.h"
#include "renderer/WireframeRenderer.h"

namespace editor {

namespace {

core::Color ColorForMotionType(physics::MotionType motion_type) {
  switch (motion_type) {
    case physics::MotionType::Static:    return core::Color(0.0f, 1.0f, 0.0f);
    case physics::MotionType::Kinematic: return core::Color(1.0f, 1.0f, 0.0f);
    case physics::MotionType::Dynamic:   return core::Color(0.0f, 1.0f, 1.0f);
  }
  return core::Color(1.0f, 1.0f, 1.0f);
}

}  // namespace

void EnqueuePhysicsGizmo(const game::GameMesh& mesh) {
  const auto& desc_opt = mesh.GetPhysicsDesc();
  if (!desc_opt) return;

  const physics::PhysicsBodyDesc& desc      = *desc_opt;
  const physics::PhysicsShapeDesc& shape    = desc.shape;
  const core::Color color = ColorForMotionType(desc.motion_type);
  const core::Mat4f& transform              = mesh.GetWorldTransform();
  renderer::WireframeRenderer& wr           = renderer::WireframeRenderer::Instance();

  switch (shape.type) {
    case physics::PhysicsShapeType::Box:
      wr.PushBox(shape.box.half_extents, color, transform);
      break;

    case physics::PhysicsShapeType::Sphere:
      wr.PushSphere(shape.sphere.radius, color, transform);
      break;

    case physics::PhysicsShapeType::Cylinder:
      wr.PushCylinder(shape.cylinder.radius, shape.cylinder.half_height,
                      color, transform);
      break;

    case physics::PhysicsShapeType::Capsule:
      wr.PushCapsule(shape.capsule.radius, shape.capsule.half_height,
                     color, transform);
      break;

    case physics::PhysicsShapeType::ConvexHull:
    case physics::PhysicsShapeType::Exact: {
      const physics::PhysicsBody* body = mesh.GetPhysicsBody();
      if (body) {
        wr.PushLineList(body->GetDebugEdges(), color, transform);
      }
      break;
    }

    case physics::PhysicsShapeType::Terrain:
      // Terrain has its own wireframe toggle; no gizmo here.
      break;
  }
}

}  // namespace editor
