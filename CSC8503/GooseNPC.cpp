#include "GooseNPC.h"
#include "PhysicsObject.h"
#include "RenderObject.h"
#include "Debug.h"

using namespace NCL;
using namespace CSC8503;

GooseNPC::GooseNPC(NavigationGrid* _grid, GameObject* _player) : StateGameObject("Goose") {
    grid = _grid;
    playerTarget = _player;
	chaseSpeed = 3.5f; // chase speed of the goose
    rootNode = nullptr;
    timeSinceLastPathCalc = 0.0f;
    BuildBehaviourTree();
}

GooseNPC::~GooseNPC() {
    delete rootNode;
}

void GooseNPC::Update(float dt) {
    if (rootNode) {
        rootNode->Execute(dt);
    }

    GameObject::Update(dt);
}

void GooseNPC::BuildBehaviourTree() {
	// behaviour: chase player
    BehaviourSequence* chaseSequence = new BehaviourSequence("Chase Sequence");

    chaseSequence->AddChild(new BehaviourAction("Chase Player", [&](float dt, BehaviourState s){ return ChasePlayer(dt); }));

    rootNode = chaseSequence;
}

BehaviourState GooseNPC::ChasePlayer(float dt) {
    if (!playerTarget || !grid) return Failure;

    Vector3 targetPos = playerTarget->GetTransform().GetPosition();
    Vector3 myPos = GetTransform().GetPosition();

    // 1. 路径计算 (每 1.0 秒更新一次，节省性能)
    timeSinceLastPathCalc += dt;
    if (timeSinceLastPathCalc > 1.0f) {
        CalculatePathTo(targetPos);
        timeSinceLastPathCalc = 0.0f;
    }

    // 2. 移动逻辑
    if (pathPoints.empty()) {
        // 如果没有路径点（就在旁边，或者寻路失败），直接向玩家移动
        Vector3 dir = (targetPos - myPos);
        dir.y = 0;
        if (Vector::Length(dir) > 2.0f) {
            GetPhysicsObject()->AddForce(Vector::Normalise(dir) * chaseSpeed);
            LookAt(targetPos, dt);
        }
        else {
            GetPhysicsObject()->SetLinearVelocity(Vector3(0, 0, 0)); // 抓到了，停下
        }
        return Ongoing;
    }

    // 获取当前路径点
    Vector3 nextWaypoint = pathPoints[0];
    nextWaypoint.y = myPos.y;
    Vector3 dir = nextWaypoint - myPos;
    dir.y = 0;
    float distToNode = Vector::Length(dir);

	// 如果到达当前路径点，移除并前往下一个, 这个数值至关重要，如果调整的不对可能会造成寻路出现问题
    if (distToNode < 20.0f) {
        pathPoints.erase(pathPoints.begin());
        return Ongoing;
    }

    // 3. 物理移动
    GetPhysicsObject()->AddForce(Vector::Normalise(dir) * chaseSpeed);
    LookAt(nextWaypoint, dt);
    
	// Draw Debug Lines
    // 1. 画出鹅到当前路标的线（短线，红色）
    Debug::DrawLine(myPos, nextWaypoint, Vector4(1, 0, 0, 1));

    // 2. 画出完整的路径规划（蓝色折线）
    // 这样你能看到从 (-60, 40) 怎么一步步连到 (-7, 80)
    for (size_t i = 0; i < pathPoints.size() - 1; ++i) {
        Vector3 a = pathPoints[i];
        Vector3 b = pathPoints[i + 1];

        // 稍微抬高一点，防止被地板遮挡
        a.y += 1;
        b.y += 1;

        Debug::DrawLine(a, b, Vector4(0, 0, 1, 1));
    }
    // 3. 画出终点位置（绿色柱子）
    Debug::DrawLine(targetPos, targetPos + Vector3(0, 10, 0), Vector4(0, 1, 0, 1));

    return Ongoing;
}

void GooseNPC::CalculatePathTo(Vector3 targetPos) {
    if (!grid) return;
    currentPath.Clear();
    pathPoints.clear();

    Vector3 startPos = GetTransform().GetPosition();

    // 使用 A* 寻路
    if (grid->FindPath(startPos, targetPos, currentPath)) {
        Vector3 pos;
        while (currentPath.PopWaypoint(pos)) {
            pathPoints.push_back(pos);
        }
    }
}

void GooseNPC::LookAt(Vector3 targetPos, float dt) {
    Vector3 dir = (targetPos - GetTransform().GetPosition());
    dir.y = 0;
    dir = Vector::Normalise(dir);

    if (Vector::LengthSquared(dir) < 0.01f) return;
    
    // 计算目标旋转
    Matrix4 lookAtMat = Matrix::View(Vector3(0, 0, 0), dir, Vector3(0, 1, 0));
    Quaternion targetOrientation = Quaternion(Matrix::Inverse(lookAtMat));

    // 获取当前旋转
    Quaternion currentOrientation = GetTransform().GetOrientation();

    // Slerp 平滑插值 (速度 5.0f 比较像鹅的转身速度)
    Quaternion newOrientation = Quaternion::Slerp(currentOrientation, targetOrientation, 5.0f * dt);

    GetTransform().SetOrientation(newOrientation);
    // 锁定角速度，防止物理碰撞导致翻滚
    GetPhysicsObject()->SetAngularVelocity(Vector3(0, 0, 0));
}