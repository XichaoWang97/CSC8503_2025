#include "GooseNPC.h"
#include "StateTransition.h"
#include "StateMachine.h"
#include "State.h"
#include "PhysicsObject.h"
#include "Debug.h"

using namespace NCL;
using namespace CSC8503;

GooseNPC::GooseNPC(NavigationGrid* _grid, GameObject* _player) : StateGameObject("Goose") {
    grid = _grid;
    playerTarget = _player;
    chaseSpeed = 15.0f; // faster chase
    patrolSpeed = 8.0f;

    // state machine
    // state 1: Wander / wait
    State* stateWander = new State([&](float dt)->void {
        this->Wander(dt);
        Debug::Print("Goose: HONK! (Wander)", Vector2(10, 50), Vector4(1, 1, 0, 1));
        }
    );

    // state 2: Chase
    State* stateChase = new State([&](float dt)->void {
        this->ChasePlayer(dt);
        Debug::Print("Goose: CHASING!", Vector2(10, 50), Vector4(1, 0, 0, 1));
        }
    );

    stateMachine->AddState(stateWander);
    stateMachine->AddState(stateChase);

    // see player -> Chase
    stateMachine->AddTransition(new StateTransition(stateWander, stateChase, [&]()->bool {
        return this->CanSeeTarget();
        })
    );

    // can not see player -> Wander
    stateMachine->AddTransition(new StateTransition(stateChase, stateWander, [&]()->bool {
        return !this->CanSeeTarget();
        })
    );
}

GooseNPC::~GooseNPC() {}

void GooseNPC::Update(float dt) {
    StateGameObject::Update(dt); // 执行状态机
}

void GooseNPC::Wander(float dt) {
    // 简单的游荡逻辑：如果没有路径，随机找一个点 (这里简化为发呆或随机移动)
    // 为了演示，我们让它就在原地转圈或者简单移动
    GetPhysicsObject()->SetAngularVelocity(Vector3(0, 1, 0));
}

void GooseNPC::ChasePlayer(float dt) {
    if (!playerTarget || !grid) return;

    // 定期重新计算路径 (每 0.5 秒或是当目标移动太远)
    timeSinceLastPathCalc += dt;
    if (timeSinceLastPathCalc > 0.5f) {
        CalculatePathTo(playerTarget->GetTransform().GetPosition());
        timeSinceLastPathCalc = 0.0f;
    }

    FollowPath(dt);
}

void GooseNPC::CalculatePathTo(Vector3 targetPos) {
    if (!grid) return;

    currentPath.Clear();
    pathPoints.clear();

    Vector3 startPos = GetTransform().GetPosition();

    // 使用 A* 寻路
    if (grid->FindPath(startPos, targetPos, currentPath)) {
        // 将 NavigationPath 转换为 vector 以便处理
        Vector3 pos;
        while (currentPath.PopWaypoint(pos)) {
            pathPoints.push_back(pos);
        }
    }
}

void GooseNPC::FollowPath(float dt) {
    if (pathPoints.empty()) return;

    // 目标是路径中的下一个点 (因为 PopWaypoint 是反向的，最后一个点其实是起点附近的点)
    // 但是 FindPath 的实现通常是从 End 到 Start 存入栈，所以 Pop 出来的是 Start -> End
    // 我们取第一个点作为目标
    Vector3 target = pathPoints[0];

    // 移动逻辑
    Vector3 dir = target - GetTransform().GetPosition();
    dir.y = 0;
    float dist = Vector::Length(dir);

    if (dist < 2.0f) {
        // 到达当前点，移除它，前往下一个
        pathPoints.erase(pathPoints.begin());
        return;
    }

    // 移动物理
    if (dist > 0) {
        Vector3 velocity = Vector::Normalise(dir) * chaseSpeed;
        GetPhysicsObject()->SetLinearVelocity(velocity);

        // 面向移动方向
        // ... (可以使用之前的 LookAt 逻辑)
    }

    // Debug: 画出路径
    for (size_t i = 0; i < pathPoints.size() - 1; ++i) {
        Debug::DrawLine(pathPoints[i], pathPoints[i + 1], Vector4(0, 1, 0, 1));
    }
}