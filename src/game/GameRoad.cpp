#include "game/GameRoad.h"

#include <loguru.hpp>

#include "core/BBox3.h"
#include "core/Vertex3D.h"
#include "physics/CollisionLayer.h"
#include "physics/MotionType.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsBodyDesc.h"
#include "physics/PhysicsShapeDesc.h"
#include "physics/PhysicsSystem.h"
#include "game/GameMaterial.h"
#include "renderer/GeometryData.h"
#include "renderer/Mesh.h"
#include "renderer/MeshInstance.h"
#include "renderer/Renderer.h"
#include "track/RoadMeshBuilder.h"

namespace game {

GameRoad::GameRoad(abstract::VideoDevice* video)
    : GameObject(GameObjectType::kRoad, core::BBox3{}),
      video_(video) {}

GameRoad::~GameRoad() {
  if (material_) material_->Release();
  DestroyPhysicsBody();
}

void GameRoad::Accept(GameObjectVisitor& visitor) {
  visitor.Visit(*this);
}

void GameRoad::OnAddedToScene() {
  in_scene_ = true;
  if (instance_)
    renderer::Renderer::Instance().AddRenderable(instance_.get());
  if (physics_body_)
    physics_body_->SetWorldTransform(GetWorldTransform());
}

void GameRoad::OnRemovedFromScene() {
  in_scene_ = false;
  if (instance_)
    renderer::Renderer::Instance().RemoveRenderable(instance_.get());
  DestroyPhysicsBody();
}

void GameRoad::OnWorldTransformUpdated() {
  if (instance_)
    instance_->SetWorldMatrix(GetWorldTransform());
  if (physics_body_)
    physics_body_->SetWorldTransform(GetWorldTransform());
}

void GameRoad::RegenerateMesh(
    const std::function<float(float, float)>& height_fn) {
  if (spline_.GetPointCount() < 2) {
    LOG_F(INFO, "GameRoad '%s': skipping RegenerateMesh — fewer than 2 control points",
          GetName().c_str());
    return;
  }

  const track::RoadMeshData raw =
      track::RoadMeshBuilder::Build(spline_, width_, samples_per_m_, height_fn);

  if (raw.vertices.empty() || raw.indices.empty()) {
    LOG_F(WARNING, "GameRoad '%s': RoadMeshBuilder returned empty data",
          GetName().c_str());
    return;
  }

  // Convert raw interleaved floats to Vertex3D.
  // Raw layout: pos(3) | normal(3) | uv(2) = 8 floats per vertex.
  const int num_vertices  = static_cast<int>(raw.vertices.size()) / 8;
  const int num_triangles = static_cast<int>(raw.indices.size())  / 3;

  // Sample spline tangents once per cross-section (each cross-section has 2
  // vertices: left and right). They are needed for the tangent-space basis.
  const int num_sections = num_vertices / 2;
  const float inv_n = 1.f / static_cast<float>(num_sections);

  std::vector<core::Vertex3D> verts;
  verts.resize(static_cast<size_t>(num_vertices));

  static const core::Vec3f kUp{0.f, 1.f, 0.f};

  for (int i = 0; i < num_sections; ++i) {
    const float t = static_cast<float>(i) * inv_n;
    const core::Vec3f tangent  = spline_.GetTangent(t).Normalized();
    // binormal (road-right direction)
    const core::Vec3f binormal = kUp.Cross(tangent).Normalized();
    const core::Vec3f normal   = kUp;

    for (int side = 0; side < 2; ++side) {
      const int vi  = i * 2 + side;
      const int raw_base = vi * 8;
      verts[vi].position = {
          raw.vertices[raw_base],
          raw.vertices[raw_base + 1],
          raw.vertices[raw_base + 2]};
      verts[vi].normal   = normal;
      verts[vi].binormal = binormal;
      verts[vi].tangent  = tangent;
      verts[vi].uv       = {raw.vertices[raw_base + 6],
                            raw.vertices[raw_base + 7]};
    }
  }

  // Remove old renderer instance before replacing GPU resources.
  if (in_scene_ && instance_)
    renderer::Renderer::Instance().RemoveRenderable(instance_.get());

  instance_.reset();
  mesh_.reset();
  geometry_.reset();

  geometry_ = std::make_unique<renderer::GeometryData>(
      video_, num_vertices, verts.data(), num_triangles, raw.indices.data());

  renderer::Material* mat = material_ ? material_->GetMaterial() : nullptr;
  mesh_ = std::make_unique<renderer::Mesh>(geometry_.get(), mat);
  instance_ = std::make_unique<renderer::MeshInstance>(
      mesh_.get(), GetWorldTransform(), /*always_visible=*/false);

  if (in_scene_)
    renderer::Renderer::Instance().AddRenderable(instance_.get());

  // Rebuild static physics body.
  DestroyPhysicsBody();

  if (physics::PhysicsSystem::IsInstanced()) {
    // Extract flat position buffer (stride 8 floats, first 3 are XYZ).
    std::vector<float> positions;
    positions.reserve(static_cast<size_t>(num_vertices) * 3);
    for (int i = 0; i < num_vertices; ++i) {
      positions.push_back(raw.vertices[i * 8]);
      positions.push_back(raw.vertices[i * 8 + 1]);
      positions.push_back(raw.vertices[i * 8 + 2]);
    }

    physics::PhysicsBodyDesc desc;
    desc.shape         = physics::PhysicsShapeDesc::MakeExact();
    desc.motion_type   = physics::MotionType::Static;
    desc.collision_layer = physics::kLayerWorld;
    desc.collision_mask  = 0xFFFF;

    physics_body_ = physics::PhysicsSystem::Instance().CreateBodyWithMesh(
        desc, nullptr, GetWorldTransform(),
        positions.data(), num_vertices,
        raw.indices.data(), static_cast<int>(raw.indices.size()));
  }
}

void GameRoad::SetMaterial(GameMaterial* mat) {
  if (mat) mat->AddRef();
  if (material_) material_->Release();
  material_ = mat;
  if (mesh_ && mat)
    mesh_->SetMaterial(mat->GetMaterial());
}

void GameRoad::DestroyPhysicsBody() {
  if (physics_body_) {
    physics::PhysicsSystem::Instance().DestroyBody(physics_body_);
    physics_body_ = nullptr;
  }
}

}  // namespace game
