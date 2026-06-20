#include "game/GameObject.h"

#include <gtest/gtest.h>

#include "core/Mat4f.h"
#include "core/MathUtils.h"

using core::Mat4f;
using core::Vec3f;

namespace {

// Minimal concrete stub — no rendering or physics plumbing needed.
class StubObject : public game::GameObject {
 public:
  explicit StubObject(const core::BBox3& bbox = core::BBox3{})
      : game::GameObject(game::GameObjectType::kMesh, bbox) {}

  void Accept(game::GameObjectVisitor&) override {}
  void OnWorldTransformUpdated() override { update_count_++; }
  void OnAddedToScene()    override {}
  void OnRemovedFromScene() override {}

  int update_count_ = 0;
};

bool MatNear(const Mat4f& a, const Mat4f& b, float eps = 1e-5f) {
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      if (!core::NearlyEqual(a(i, j), b(i, j), eps)) return false;
  return true;
}

}  // namespace

// ---- SetLocalTransform / SetWorldTransform no-parent ---------------------

TEST(GameObjectHierarchyTest, SetLocalTransformNoParentEqualsWorld) {
  StubObject obj;
  Mat4f t = Mat4f::Translation({1.f, 2.f, 3.f});
  obj.SetLocalTransform(t);
  EXPECT_TRUE(MatNear(obj.GetLocalTransform(), t));
  EXPECT_TRUE(MatNear(obj.GetWorldTransform(), t));
}

TEST(GameObjectHierarchyTest, SetWorldTransformNoParentEqualsLocal) {
  StubObject obj;
  Mat4f t = Mat4f::Translation({4.f, 5.f, 6.f});
  obj.SetWorldTransform(t);
  EXPECT_TRUE(MatNear(obj.GetWorldTransform(), t));
  EXPECT_TRUE(MatNear(obj.GetLocalTransform(), t));
}

TEST(GameObjectHierarchyTest, SetLocalTransformCallsOnWorldTransformUpdated) {
  StubObject obj;
  obj.SetLocalTransform(Mat4f::Translation({1.f, 0.f, 0.f}));
  EXPECT_EQ(obj.update_count_, 1);
}

// ---- AddChild / RemoveChild ---------------------------------------------

TEST(GameObjectHierarchyTest, AddChildLinksParent) {
  StubObject parent, child;
  parent.AddChild(&child);
  EXPECT_EQ(child.GetParent(), &parent);
  ASSERT_EQ(parent.GetChildren().size(), 1u);
  EXPECT_EQ(parent.GetChildren()[0], &child);
}

TEST(GameObjectHierarchyTest, AddChildPreservesChildWorldPosition) {
  StubObject parent, child;
  Mat4f parent_t = Mat4f::Translation({10.f, 0.f, 0.f});
  Mat4f child_t  = Mat4f::Translation({15.f, 0.f, 0.f});
  parent.SetWorldTransform(parent_t);
  child.SetWorldTransform(child_t);

  parent.AddChild(&child);

  // Child world transform must stay at (15, 0, 0).
  EXPECT_TRUE(MatNear(child.GetWorldTransform(), child_t));
  // Child local transform must reflect relative offset (5, 0, 0).
  EXPECT_TRUE(MatNear(child.GetLocalTransform(),
                      Mat4f::Translation({5.f, 0.f, 0.f})));
}

TEST(GameObjectHierarchyTest, RemoveChildUnlinksAndRetainsWorldTransform) {
  StubObject parent, child;
  parent.SetWorldTransform(Mat4f::Translation({10.f, 0.f, 0.f}));
  child.SetWorldTransform(Mat4f::Translation({15.f, 0.f, 0.f}));
  parent.AddChild(&child);
  parent.RemoveChild(&child);

  EXPECT_EQ(child.GetParent(), nullptr);
  EXPECT_TRUE(parent.GetChildren().empty());
  // World transform is retained.
  EXPECT_TRUE(MatNear(child.GetWorldTransform(),
                      Mat4f::Translation({15.f, 0.f, 0.f})));
  // Local equals world after detach.
  EXPECT_TRUE(MatNear(child.GetLocalTransform(),
                      Mat4f::Translation({15.f, 0.f, 0.f})));
}

// ---- World transform propagation ----------------------------------------

TEST(GameObjectHierarchyTest, ParentMovePropagatesToChild) {
  StubObject parent, child;
  parent.AddChild(&child);

  // Place parent; child local is identity, so child world == parent world.
  parent.SetLocalTransform(Mat4f::Translation({5.f, 0.f, 0.f}));

  EXPECT_TRUE(MatNear(child.GetWorldTransform(),
                      Mat4f::Translation({5.f, 0.f, 0.f})));
}

TEST(GameObjectHierarchyTest, PropagationTwoLevelsDeep) {
  StubObject grand, parent, child;
  grand.AddChild(&parent);
  parent.AddChild(&child);

  grand.SetLocalTransform(Mat4f::Translation({1.f, 0.f, 0.f}));

  // parent and child both shift.
  EXPECT_TRUE(MatNear(parent.GetWorldTransform(),
                      Mat4f::Translation({1.f, 0.f, 0.f})));
  EXPECT_TRUE(MatNear(child.GetWorldTransform(),
                      Mat4f::Translation({1.f, 0.f, 0.f})));
}

TEST(GameObjectHierarchyTest, SetWorldTransformWithParentDerivesLocalCorrectly) {
  StubObject parent, child;
  parent.SetWorldTransform(Mat4f::Translation({10.f, 0.f, 0.f}));
  parent.AddChild(&child);

  // Move child to absolute world position (14, 0, 0) via SetWorldTransform.
  child.SetWorldTransform(Mat4f::Translation({14.f, 0.f, 0.f}));

  EXPECT_TRUE(MatNear(child.GetWorldTransform(),
                      Mat4f::Translation({14.f, 0.f, 0.f})));
  EXPECT_TRUE(MatNear(child.GetLocalTransform(),
                      Mat4f::Translation({4.f, 0.f, 0.f})));
}

// ---- Reparent ------------------------------------------------------------

TEST(GameObjectHierarchyTest, ReparentPreservesWorldTransform) {
  StubObject a, b, child;
  a.SetWorldTransform(Mat4f::Translation({0.f,  0.f, 0.f}));
  b.SetWorldTransform(Mat4f::Translation({20.f, 0.f, 0.f}));
  child.SetWorldTransform(Mat4f::Translation({5.f, 0.f, 0.f}));
  a.AddChild(&child);

  child.Reparent(&b);

  EXPECT_EQ(child.GetParent(), &b);
  EXPECT_EQ(b.GetChildren().size(), 1u);
  EXPECT_TRUE(a.GetChildren().empty());
  // World position is preserved.
  EXPECT_TRUE(MatNear(child.GetWorldTransform(),
                      Mat4f::Translation({5.f, 0.f, 0.f})));
}

TEST(GameObjectHierarchyTest, ReparentToNullDetaches) {
  StubObject parent, child;
  parent.AddChild(&child);
  child.Reparent(nullptr);

  EXPECT_EQ(child.GetParent(), nullptr);
  EXPECT_TRUE(parent.GetChildren().empty());
}

// ---- Physics path (SetWorldTransformPhysics) ----------------------------
// Exposed via a thin subclass that wraps the protected method.

class PhysicsStub : public StubObject {
 public:
  void SimulatePhysicsUpdate(const Mat4f& t) {
    SetWorldTransformPhysics(t);
  }
};

TEST(GameObjectHierarchyTest, PhysicsUpdateDoesNotBackPropagateToParent) {
  StubObject parent;
  PhysicsStub child;
  parent.SetWorldTransform(Mat4f::Translation({0.f, 0.f, 0.f}));
  parent.AddChild(&child);

  // Physics moves only the child — parent must stay at origin.
  child.SimulatePhysicsUpdate(Mat4f::Translation({3.f, 0.f, 0.f}));

  EXPECT_TRUE(MatNear(child.GetWorldTransform(),
                      Mat4f::Translation({3.f, 0.f, 0.f})));
  EXPECT_TRUE(MatNear(parent.GetWorldTransform(), Mat4f::kIdentity));
}

TEST(GameObjectHierarchyTest, PhysicsUpdatePropagatesDownToChildren) {
  PhysicsStub middle;
  StubObject grandchild;
  middle.AddChild(&grandchild);

  middle.SimulatePhysicsUpdate(Mat4f::Translation({7.f, 0.f, 0.f}));

  // grandchild's local is identity, so its world follows middle.
  EXPECT_TRUE(MatNear(grandchild.GetWorldTransform(),
                      Mat4f::Translation({7.f, 0.f, 0.f})));
}
