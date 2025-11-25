#include "StateGameObject.h"
#include "StateTransition.h"
#include "StateMachine.h"
#include "State.h"
#include "PhysicsObject.h"
#include "Debug.h"

using namespace NCL;
using namespace CSC8503;

StateGameObject::StateGameObject(const std::string& objectName) {
	name = objectName;
	counter = 0.0f;
	stateMachine = new StateMachine();

	// 1. 创建状态
	State* statePatrol = new State([&](float dt)->void {
		this->Patrol(dt);
		}
	);

	State* stateChase = new State([&](float dt)->void {
		this->Chase(dt);
		}
	);

	stateMachine->AddState(statePatrol);
	stateMachine->AddState(stateChase);

	// 2. 创建转换 (Transitions)

	// 条件 A: Patrol -> Chase (如果玩家距离 < 15.0f)
	// 注意：任务 1.2 会把这里升级为射线检测
	stateMachine->AddTransition(new StateTransition(statePatrol, stateChase, [&]()->bool {
		if (!playerTarget) return false;
		float dist = Vector::Length(playerTarget->GetTransform().GetPosition() - this->GetTransform().GetPosition());
		return dist < 15.0f;
		})
	);

	// 条件 B: Chase -> Patrol (如果玩家距离 > 20.0f，追丢了)
	stateMachine->AddTransition(new StateTransition(stateChase, statePatrol, [&]()->bool {
		if (!playerTarget) return true; // 没目标就回去巡逻
		float dist = Vector::Length(playerTarget->GetTransform().GetPosition() - this->GetTransform().GetPosition());
		return dist > 20.0f;
		})
	);
}

StateGameObject::~StateGameObject() {
	delete stateMachine;
}

void StateGameObject::Update(float dt) {
	stateMachine->Update(dt);
}

// --- 状态行为实现 ---
void StateGameObject::Patrol(float dt) {
	if (patrolPath.empty()) return;

	// 获取当前目标点
	Vector3 targetPos = patrolPath[currentWaypointIndex];

	// 移动
	MoveTo(targetPos, patrolSpeed, dt);

	// 距离检测：是否到达目标点？
	float dist = Vector::Length(targetPos - GetTransform().GetPosition());
	if (dist < 2.0f) {
		// 切换到下一个点
		currentWaypointIndex = (currentWaypointIndex + 1) % patrolPath.size();
	}

	// 调试绘制：画出巡逻路径
	Debug::DrawLine(GetTransform().GetPosition(), targetPos, Vector4(1, 1, 0, 1)); // 黄线
}

void StateGameObject::Chase(float dt) {
	if (!playerTarget) return;

	// 获取玩家位置
	Vector3 targetPos = playerTarget->GetTransform().GetPosition();

	// 移动
	MoveTo(targetPos, chaseSpeed, dt);

	// 调试绘制：画出追逐线
	Debug::DrawLine(GetTransform().GetPosition(), targetPos, Vector4(1, 0, 0, 1)); // 红线
}

void StateGameObject::MoveTo(Vector3 targetPos, float speed, float dt) {
	Vector3 pos = GetTransform().GetPosition();
	Vector3 direction = targetPos - pos;

	// 忽略高度差异，只在水平面上移动
	direction.y = 0;

	if (Vector::Length(direction) > 0.1f) {
		Vector3 velocity = Vector::Normalise(direction) * speed;
		GetPhysicsObject()->SetLinearVelocity(velocity);

		// 可选：让 AI 面向移动方向 (如果想做的好看点)
		// Matrix4 lookAt = Matrix::View(Vector3(0,0,0), -velocity, Vector3(0,1,0));
		// GetTransform().SetOrientation(Quaternion(Matrix::Inverse(lookAt)));
	}
	else {
		GetPhysicsObject()->SetLinearVelocity(Vector3(0, 0, 0));
	}
}