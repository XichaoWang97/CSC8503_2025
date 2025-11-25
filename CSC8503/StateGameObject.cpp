#include "StateGameObject.h"
#include "StateTransition.h"
#include "StateMachine.h"
#include "State.h"
#include "PhysicsObject.h"
#include "Debug.h"
#include "GameWorld.h"
#include "Ray.h"
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


	// --- 任务 1.2 修改: 使用视线检测作为转换条件 ---
	// Patrol -> Chase
	stateMachine->AddTransition(new StateTransition(statePatrol, stateChase, [&]()->bool {
		return this->CanSeeTarget();
		})
	);

	// Chase -> Patrol (如果看不见或者距离太远)
	stateMachine->AddTransition(new StateTransition(stateChase, statePatrol, [&]()->bool {
		return !this->CanSeeTarget();
		})
	);
}

StateGameObject::~StateGameObject() {
	delete stateMachine;
}

void StateGameObject::Update(float dt) {
	stateMachine->Update(dt);
}

// --- 任务 1.3 修改: 使用物理碰撞回调 ---
void StateGameObject::OnCollisionBegin(GameObject* otherObject) {
	// 只有当撞到的对象是我们的目标（玩家）时，才触发逻辑
	if (otherObject == playerTarget) {
		// 重置玩家位置
		playerTarget->GetTransform().SetPosition(Vector3(-60, 5, 60));

		// 清零玩家速度
		if (playerTarget->GetPhysicsObject()) {
			playerTarget->GetPhysicsObject()->ClearForces();
			playerTarget->GetPhysicsObject()->SetLinearVelocity(Vector3(0, 0, 0));
			playerTarget->GetPhysicsObject()->SetAngularVelocity(Vector3(0, 0, 0));
		}
	}
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

	direction.y = 0;

	if (Vector::Length(direction) > 0.1f) {
		Vector3 velocity = Vector::Normalise(direction) * speed;

		// --- 修复：保留当前的垂直速度 (重力) ---
		Vector3 currentVelocity = GetPhysicsObject()->GetLinearVelocity();
		velocity.y = currentVelocity.y;

		GetPhysicsObject()->SetLinearVelocity(velocity);
	}
	else {
		// 停止时也保留重力
		Vector3 currentVelocity = GetPhysicsObject()->GetLinearVelocity();
		GetPhysicsObject()->SetLinearVelocity(Vector3(0, currentVelocity.y, 0));
	}
}

// --- 任务 1.2: 视线检测实现 ---
bool StateGameObject::CanSeeTarget() {
	if (!playerTarget || !gameWorld) return false;

	Vector3 aiPos = GetTransform().GetPosition();
	Vector3 playerPos = playerTarget->GetTransform().GetPosition();

	// 距离检测
	float dist = Vector::Length(playerPos - aiPos);
	if (dist > 30.0f) { // 增加视野距离
		return false;
	}

	// 射线检测
	// 从 AI 中心稍微偏上一点的位置发射，指向玩家中心
	// 避免射线直接打到地面
	Vector3 rayOrigin = aiPos + Vector3(0, 2.0f, 0);
	Vector3 targetPoint = playerPos + Vector3(0, 1.0f, 0); // 瞄准玩家胸部

	Vector3 rayDir = targetPoint - rayOrigin;
	rayDir = Vector::Normalise(rayDir); // Ray 需要归一化的方向

	Ray ray(rayOrigin, rayDir);
	RayCollision collision;

	if (gameWorld->Raycast(ray, collision, true, this)) {
		// 调试：画出射线
		// 如果打中玩家，画绿线；否则画红线
		if (collision.node == playerTarget) {
			Debug::DrawLine(rayOrigin, collision.collidedAt, Vector4(0, 1, 0, 1)); // 绿色：看见了
			return true;
		}
		else {
			Debug::DrawLine(rayOrigin, collision.collidedAt, Vector4(1, 0, 0, 1)); // 红色：被挡住
			// 这里可以打印一下到底打到了什么
			// std::cout << "Ray hit: " << ((GameObject*)collision.node)->GetName() << std::endl;
			return false;
		}
	}

	return false;
}