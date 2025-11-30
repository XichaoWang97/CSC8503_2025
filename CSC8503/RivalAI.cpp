#include "RivalAI.h"
#include "GameWorld.h"
#include "PhysicsObject.h"
#include "RenderObject.h"
#include "FragileGameObject.h" // 假设你有这个头文件 (任务 1.4)
#include "Debug.h"
#include "SphereVolume.h"
#include "BehaviourAction.h"
#include "BehaviourSelector.h"
#include "BehaviourSequence.h"
using namespace NCL;
using namespace CSC8503;

RivalAI::RivalAI(NavigationGrid* _grid) : GameObject("RivalAI") {
    grid = _grid;
    currentTarget = nullptr;
    rootNode = nullptr;
    moveSpeed = 10.0f;
    throwCooldown = 0.0f;

    BuildBehaviourTree();
}

RivalAI::~RivalAI() {
    delete rootNode;
}

void RivalAI::Update(float dt) {
    if (throwCooldown > 0) throwCooldown -= dt;

    if (rootNode) {
        rootNode->Execute(dt);
    }
}

void RivalAI::BuildBehaviourTree() {
    // Root (Selector)
    //   -> Sequence (Attack): HaveTarget? -> InRange? -> ThrowStone
    //   -> Sequence (Chase): HaveTarget? -> MoveToTarget
    //   -> Action (Search): FindTarget

    BehaviourAction* findItem = new BehaviourAction("Find Item", [&](float dt, BehaviourState state) -> BehaviourState {
        return FindTargetPacket(dt);
        }
    );

    BehaviourAction* moveToItem = new BehaviourAction("Move To Item", [&](float dt, BehaviourState state) -> BehaviourState {
        return MoveToTarget(dt);
        }
    );

    BehaviourAction* throwStone = new BehaviourAction("Throw Stone", [&](float dt, BehaviourState state) -> BehaviourState {
        return ThrowStone(dt);
        }
    );

    // 组合逻辑
    BehaviourSequence* sequenceAttack = new BehaviourSequence("Attack Sequence");
    sequenceAttack->AddChild(throwStone); // 简化：只要执行这个节点，内部会判断是否在范围内

    BehaviourSequence* sequenceChase = new BehaviourSequence("Chase Sequence");
    sequenceChase->AddChild(moveToItem);

    BehaviourSelector* rootSelector = new BehaviourSelector("Root Selector");
    // 逻辑顺序：先尝试攻击 -> 如果不行(比如没目标或太远)尝试移动 -> 如果没目标尝试寻找
    rootSelector->AddChild(sequenceAttack);
    rootSelector->AddChild(sequenceChase);
    rootSelector->AddChild(findItem);

    rootNode = rootSelector;
}

BehaviourState RivalAI::FindTargetPacket(float dt) {
    if (currentTarget && currentTarget->IsActive()) return Success; // 已经有目标

    // 遍历世界寻找 FragileGameObject
    // 注意：这里需要遍历 GameWorld，简单的做法是使用 world iterator
    // 或者在 UpdateGame 里传进来，为了简化，我们假设 GetGameWorld() 可用

    if (!gameWorld) return Failure;

    float minDist = 9999.0f;
    GameObject* bestObj = nullptr;
    Vector3 myPos = GetTransform().GetPosition();

    gameWorld->OperateOnContents([&](GameObject* o) {
        // 使用 dynamic_cast 检查是否是易碎品
        // 如果你没有启用 RTTI，可以用名字判断 o->GetName() == "FragilePackage"
        if (o->GetName() == "FragilePackage" && o->IsActive()) {
            float dist = Vector::Length(o->GetTransform().GetPosition() - myPos);
            if (dist < minDist) {
                minDist = dist;
                bestObj = o;
            }
        }
        });

    if (bestObj) {
        currentTarget = bestObj;
        Debug::Print("Rival found target!", Vector2(10, 60), Vector4(1, 0, 1, 1));
        return Success;
    }

    return Failure;
}

BehaviourState RivalAI::MoveToTarget(float dt) {
    if (!currentTarget) return Failure;

    Vector3 targetPos = currentTarget->GetTransform().GetPosition();
    Vector3 myPos = GetTransform().GetPosition();
    float dist = Vector::Length(targetPos - myPos);

    // 如果距离足够近，可以停止移动并准备攻击
    if (dist < 15.0f) {
        GetPhysicsObject()->SetLinearVelocity(Vector3(0, 0, 0));
        return Failure; // 移动结束，把控制权交给 Attack Sequence
    }

    // 简单的路径计算逻辑 (类似 Goose)
    if (pathPoints.empty()) {
        CalculatePath();
    }

    // 跟随路径 (简化版)
    if (!pathPoints.empty()) {
        Vector3 nextWaypoint = pathPoints[0];
        Vector3 dir = nextWaypoint - myPos;
        dir.y = 0;

        if (Vector::Length(dir) < 2.0f) {
            pathPoints.erase(pathPoints.begin());
            return Ongoing;
        }

        GetPhysicsObject()->SetLinearVelocity(Vector::Normalise(dir) * moveSpeed);
        return Ongoing;
    }

    // 如果没有路径点但很远，重新计算
    CalculatePath();
    return Ongoing;
}

void RivalAI::CalculatePath() {
    if (!currentTarget || !grid) return;
    NavigationPath outPath;
    Vector3 target = currentTarget->GetTransform().GetPosition();

    bool found = grid->FindPath(GetTransform().GetPosition(), target, outPath);

    pathPoints.clear();
    Vector3 pos;
    while (outPath.PopWaypoint(pos)) {
        pathPoints.push_back(pos);
    }
}

// --- 任务 3.4: 投掷石头机制 ---
BehaviourState RivalAI::ThrowStone(float dt) {
    if (!currentTarget) return Failure;

    float dist = Vector::Length(currentTarget->GetTransform().GetPosition() - GetTransform().GetPosition());
    if (dist > 20.0f) return Failure; // 太远了，不能扔

    if (throwCooldown > 0) return Ongoing; // 冷却中

    // 执行投掷
    Vector3 myPos = GetTransform().GetPosition();
    Vector3 targetPos = currentTarget->GetTransform().GetPosition();

    // 1. 生成石头
    GameObject* stone = new GameObject("Stone");
    stone->GetTransform().SetPosition(myPos + Vector3(0, 2, 0)).SetScale(Vector3(0.5f, 0.5f, 0.5f));
    stone->SetBoundingVolume(new SphereVolume(0.5f));

    // 需要 RenderObject (用个简单的小球)
    // 注意：这里需要访问 mesh 和 shader，你可以从 GameWorld 获取或者为了简化直接不渲染(如果不可见)，
    // 但为了看清楚效果，建议在 NetworkedGame 里把 Mesh 传进来，或者暂时创建一个无渲染的物理实体。
    // stone->SetRenderObject(...) 

    stone->SetPhysicsObject(new PhysicsObject(stone->GetTransform(), stone->GetBoundingVolume()));
    stone->GetPhysicsObject()->SetInverseMass(10.0f); // 重一点
    stone->GetPhysicsObject()->InitSphereInertia();

    gameWorld->AddGameObject(stone);

    // 2. 计算方向并施加力
    Vector3 dir = (targetPos - myPos);
    dir.y += 5.0f; // 稍微往上抛一点
    dir = Vector::Normalise(dir);

    float throwForce = 800.0f;
    stone->GetPhysicsObject()->AddForce(dir * throwForce);

    throwCooldown = 3.0f; // 3秒冷却

    Debug::Print("Rival Throws Stone!", Vector2(10, 65), Vector4(1, 0, 0, 1));

    // 如果是网络游戏，石头可能也需要同步，这里暂时作为仅服务器实体，或者自动销毁
    return Success;
}