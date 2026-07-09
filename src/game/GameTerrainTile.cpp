#include "game/GameTerrainTile.h"

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
#include "track/TerrainTile.h"

namespace game {

namespace {

// Half the vertical thickness of the tile's static physics collider. The box
// is offset upward by this amount (see RegenerateMesh) so its top face clears
// typical terrain height variation across the footprint, guaranteeing vehicle
// wheel raycasts hit the tile before the terrain heightfield beneath it.
constexpr float kColliderHalfHeight = 0.1f;

}  // namespace

GameTerrainTile::GameTerrainTile(abstract::VideoDevice* video)
    : GameObject(GameObjectType::kTerrainTile, core::BBox3{}),
      video_(video) {}

GameTerrainTile::~GameTerrainTile() {
  if (material_) material_->Release();
  DestroyPhysicsBody();
}

void GameTerrainTile::Accept(GameObjectVisitor& visitor) {
  visitor.Visit(*this);
}

void GameTerrainTile::OnAddedToScene() {
  in_scene_ = true;
  if (instance_)
    renderer::Renderer::Instance().AddRenderable(instance_.get());
  if (physics_body_)
    physics_body_->SetWorldTransform(GetWorldTransform());
}

void GameTerrainTile::OnRemovedFromScene() {
  in_scene_ = false;
  if (instance_)
    renderer::Renderer::Instance().RemoveRenderable(instance_.get());
  DestroyPhysicsBody();
}

void GameTerrainTile::OnWorldTransformUpdated() {
  if (instance_)
    instance_->SetWorldMatrix(GetWorldTransform());
  if (physics_body_)
    physics_body_->SetWorldTransform(GetWorldTransform());
}

void GameTerrainTile::RegenerateMesh() {
  const track::TileMeshData raw =
      track::TerrainTile::BuildQuad(desc_.width, desc_.length);

  if (in_scene_ && instance_)
    renderer::Renderer::Instance().RemoveRenderable(instance_.get());

  instance_.reset();
  mesh_.reset();
  geometry_.reset();

  geometry_ = std::make_unique<renderer::GeometryData>(
      video_, static_cast<int>(raw.vertices.size()), raw.vertices.data(),
      static_cast<int>(raw.indices.size() / 3), raw.indices.data());

  renderer::Material* mat = material_ ? material_->GetMaterial() : nullptr;
  mesh_ = std::make_unique<renderer::Mesh>(geometry_.get(), mat);
  instance_ = std::make_unique<renderer::MeshInstance>(
      mesh_.get(), GetWorldTransform(), /*always_visible=*/false);

  if (in_scene_)
    renderer::Renderer::Instance().AddRenderable(instance_.get());

  const float hw = desc_.width  * 0.5f;
  const float hl = desc_.length * 0.5f;
  SetLocalBBox(core::BBox3(
      core::Vec3f(-hw, -0.01f, -hl),
      core::Vec3f( hw, 2.f * kColliderHalfHeight, hl)));

  // Rebuild static physics body: a thin box straddling the tile's placement
  // height, offset upward so its top face clears terrain variation across
  // the footprint (see kColliderHalfHeight).
  DestroyPhysicsBody();

  if (physics::PhysicsSystem::IsInstanced()) {
    physics::PhysicsBodyDesc pdesc;
    pdesc.shape = physics::PhysicsShapeDesc::MakeBox(
        core::Vec3f(hw, kColliderHalfHeight, hl));
    pdesc.shape.center_offset = core::Vec3f(0.f, kColliderHalfHeight, 0.f);
    pdesc.material         = desc_.surface;
    pdesc.motion_type      = physics::MotionType::Static;
    pdesc.collision_layer  = physics::kLayerWorld;
    pdesc.collision_mask   = 0xFFFF;

    physics_body_ = physics::PhysicsSystem::Instance().CreateBody(
        pdesc, nullptr, GetWorldTransform());
  }
}

void GameTerrainTile::SetMaterial(GameMaterial* mat) {
  if (mat) mat->AddRef();
  if (material_) material_->Release();
  material_ = mat;
  if (mesh_ && mat)
    mesh_->SetMaterial(mat->GetMaterial());
}

void GameTerrainTile::DestroyPhysicsBody() {
  if (physics_body_) {
    physics::PhysicsSystem::Instance().DestroyBody(physics_body_);
    physics_body_ = nullptr;
  }
}

}  // namespace game
