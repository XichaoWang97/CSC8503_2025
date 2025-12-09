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

	// Create state
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

	// Patrol -> Chase
	stateMachine->AddTransition(new StateTransition(statePatrol, stateChase, [&]()->bool {
		return this->CanSeeTarget();
		})
	);

	// Chase -> Patrol
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
	playerTarget = GetClosestPlayer();
}


void StateGameObject::OnCollisionBegin(GameObject* otherObject) {
	if (this->GetName() == "Goose") return; // logic of goose is in its own class

	if (otherObject == playerTarget) {
		// reset player position
		if (playerTarget->GetHeldItem()) {
			playerTarget->ThrowHeldItem(Vector3(0, 0, 0)); // there is something, so throw
		}
		playerTarget->GetTransform().SetPosition(playerTarget->GetInitPosition());

		// clear velocity
		if (playerTarget->GetPhysicsObject()) {
			playerTarget->GetPhysicsObject()->ClearForces();
			playerTarget->GetPhysicsObject()->SetLinearVelocity(Vector3(0, 0, 0));
			playerTarget->GetPhysicsObject()->SetAngularVelocity(Vector3(0, 0, 0));
		}
	}
}

void StateGameObject::Patrol(float dt) {
	if (patrolPath.empty()) return;

	// Get target pos
	Vector3 targetPos = patrolPath[currentWaypointIndex];

	// move
	MoveTo(targetPos, patrolSpeed, dt);

	// check distance
	float dist = Vector::Length(targetPos - GetTransform().GetPosition());
	if (dist < 2.0f) {
		currentWaypointIndex = (currentWaypointIndex + 1) % patrolPath.size();
	}

	// Draw patrol line
	Debug::DrawLine(GetTransform().GetPosition(), targetPos, Debug::YELLOW);
}

void StateGameObject::Chase(float dt) {
	if (!playerTarget) return;

	Vector3 targetPos = playerTarget->GetTransform().GetPosition();

	MoveTo(targetPos, chaseSpeed, dt);

	// Draw chase line
	Debug::DrawLine(GetTransform().GetPosition(), targetPos, Debug::RED);
}

void StateGameObject::MoveTo(Vector3 targetPos, float speed, float dt) {
	Vector3 pos = GetTransform().GetPosition();
	Vector3 direction = targetPos - pos;

	direction.y = 0;

	if (Vector::Length(direction) > 0.1f) {
		Vector3 velocity = Vector::Normalise(direction) * speed;

		Vector3 currentVelocity = GetPhysicsObject()->GetLinearVelocity();
		velocity.y = currentVelocity.y;

		GetPhysicsObject()->SetLinearVelocity(velocity);
	}
	else {
		Vector3 currentVelocity = GetPhysicsObject()->GetLinearVelocity();
		GetPhysicsObject()->SetLinearVelocity(Vector3(0, currentVelocity.y, 0));
	}
}

// view
bool StateGameObject::CanSeeTarget() {
	if (!playerTarget || !gameWorld) return false;

	Vector3 aiPos = GetTransform().GetPosition();
	Vector3 playerPos = playerTarget->GetTransform().GetPosition();

	float dist = Vector::Length(playerPos - aiPos);
	if (dist > 30.0f) {
		return false;
	}

	// ray check
	Vector3 rayOrigin = aiPos;
	Vector3 targetPoint = playerPos;

	Vector3 rayDir = targetPoint - rayOrigin;
	rayDir = Vector::Normalise(rayDir);

	Ray ray(rayOrigin, rayDir);
	RayCollision collision;

	if (gameWorld->Raycast(ray, collision, true, this)) {
		// see player or not
		if (collision.node == playerTarget) {
			Debug::DrawLine(rayOrigin, collision.collidedAt, Debug::GREEN); // see
			return true;
		}
		else {
			Debug::DrawLine(rayOrigin, collision.collidedAt, Debug::RED); // not see
			return false;
		}
	}

	return false;
}

Player* StateGameObject::GetClosestPlayer() {
	if (!allPlayers || allPlayers->empty()) return nullptr;

	Player* closestP = nullptr;
	float minDistSq = FLT_MAX;
	Vector3 myPos = GetTransform().GetPosition();

	for (Player* p : *allPlayers) {
		if (!p || !p->IsActive()) continue; // ignore not active players

		float distSq = Vector::LengthSquared(p->GetTransform().GetPosition() - myPos);

		if (distSq < minDistSq) {
			minDistSq = distSq;
			closestP = p;
		}
	}
	return closestP;
}