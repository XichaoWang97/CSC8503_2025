#include "FragileGameObject.h"
#include "PhysicsObject.h"
#include "RenderObject.h"
#include "PhysicsSystem.h"
#include "Debug.h"
#include <iostream>

using namespace NCL;
using namespace CSC8503;
using namespace Maths;

FragileGameObject::FragileGameObject(const std::string& name, const Vector3& position,
    Rendering::Mesh* mesh, GameTechMaterial material, Vector4 colour) : GameObject(name) {
	
	// set properties
    initialPosition = position;
    maxHealth = 100.0f;
    health = maxHealth;
    isBroken = false;
	isAttached = false;
    collectionCount = 0;

	// set physics volume
    SphereVolume* volume = new SphereVolume(1.0f);
    SetBoundingVolume(volume);

	// set position
    GetTransform().SetScale(Vector3(1, 1, 1)).SetPosition(position);

	// set render object
    SetRenderObject(new RenderObject(GetTransform(), mesh, material));
    GetRenderObject()->SetColour(colour);

    PhysicsObject* physicsObj = new PhysicsObject(GetTransform(), GetBoundingVolume());
    physicsObj->SetInverseMass(1.0f);
    physicsObj->InitSphereInertia();
	physicsObj->SetElasticity(0.3f); // low bounciness to prevent excessive bouncing

    SetPhysicsObject(physicsObj);
}

FragileGameObject::~FragileGameObject() {
}

// Deal with respawn timer
void FragileGameObject::Update(float dt) {
    if (isBroken) {
        timer -= dt;
        GetTransform().SetPosition(Vector3(0, -9999, 0));
        Debug::Print("Package respawning in: " + std::to_string((int)timer + 1), Vector2(40, 55), Vector4(1, 0, 0, 1));

        if (timer <= 0.0f) {
            Reset();
        }
    }
}

void FragileGameObject::OnCollisionBegin(GameObject* otherObject) {
    if (isBroken) return;

    PhysicsObject* myPhys = GetPhysicsObject();
    if (!myPhys) return;

	// get other object's velocity
    Vector3 otherVel = Vector3(0, 0, 0);
    if (otherObject->GetPhysicsObject()) {
        otherVel = otherObject->GetPhysicsObject()->GetLinearVelocity();
    }

	// calculate relative velocity
    Vector3 myVel = myPhys->GetLinearVelocity();
    Vector3 relVel = myVel - otherVel;
    float impactSpeed = Vector::Length(relVel);

	// Setting threshold: If velocity is bigger that threshold, package will take damage.
	// set a safe threshold to avoid player collisions and constraint causing damage
    float damageThreshold = 40.0f;

    if (impactSpeed > damageThreshold) {
		// calculate damage
        float damage = (impactSpeed - damageThreshold) * 2.0f;
        health -= damage;

        if (health <= 0) {
            health = 0;
            isBroken = true;
            timer = 5.0f;
        }
        else {
			// get colour based on health
            if (GetRenderObject()) {
				// from blue (healthy) to red (damaged)
                float healthRatio = health / maxHealth;
                GetRenderObject()->SetColour(Vector4(1.0f - healthRatio, healthRatio * 0.5f, healthRatio, 1));
            }
        }
    }
}

void FragileGameObject::Reset() {
    isBroken = false;
    health = maxHealth;
    timer = 0.0f;

	// reset position and orientation
    GetTransform().SetPosition(initialPosition);
    GetTransform().SetOrientation(Quaternion(0.0f, 0.0f, 0.0f, 1.0f));
	// colour
    if (GetRenderObject()) {
        GetRenderObject()->SetColour(Vector4(0, 0, 1, 1));
    }
	// physics
    if (GetPhysicsObject()) {
        GetPhysicsObject()->ClearForces();
        GetPhysicsObject()->SetLinearVelocity(Vector3(0, 0, 0));
        GetPhysicsObject()->SetAngularVelocity(Vector3(0, 0, 0));
        GetPhysicsObject()->SetInverseMass(1.0f);
    }
}