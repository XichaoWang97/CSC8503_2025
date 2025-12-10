#pragma once
#include "GameObject.h"
#include "GameWorld.h"
#include "PositionConstraint.h"
#include "FragileGameObject.h"
#include "PhysicsSystem.h"
#include "PhysicsObject.h"
#include "Ray.h"
#include "Window.h"
#include "Debug.h"

namespace NCL::CSC8503 {
    class GameCharacter : public GameObject {
    public:
        GameCharacter(std::string name, GameWorld* world) : GameObject(name) {
            gameWorld = world;
            heldItem = nullptr;
            grabConstraint = nullptr;
            fragilePackage = nullptr;
        }

        ~GameCharacter() {
            DropHeldItem();
        }

		// set fragile package
        void SetFragilePackage(FragileGameObject* package) {
            fragilePackage = package;
		}

        // Grab
        void TryGrab(Vector3 aimDir) {
            Vector3 pos = GetTransform().GetPosition();
            Vector3 startPos = pos + Vector3(0, 0.5f, 0);
            Ray ray(startPos, aimDir);
            RayCollision collision;

            // grab distance: 30m
            if (gameWorld->Raycast(ray, collision, true, this)) {
                GameObject* target = (GameObject*)collision.node;
                float dist = collision.rayDistance;

                if (dist < 20.0f) {
                    if (target->GetName() == "Stone" || target->GetName()=="CubeStone") {
                        AttachItem(target);
                    }
                    if (target->GetName() == "FragilePackage") {
						// if package is not attached, it can be grabbed
                        if (fragilePackage->GetAttached() == false) {
                            AttachItem(target);
							fragilePackage->SetAttached(true);
                            if (this->GetName() == "Player") {
                                fragilePackage->SetHeldByPlayer(1); // it is held by player
                            }
                        }
                        else if (fragilePackage->GetAttached() == true && 
                            this->GetName() == "Player" && fragilePackage->GetHeldByPlayer() > 0) {
                            // players can hold the package at the same time
                            AttachItem(target);
                            fragilePackage->SetAttached(true);
                            fragilePackage->SetHeldByPlayer(1);
                        }
                    }
                }
            }
            Debug::DrawLine(startPos, startPos + (aimDir * 10.0f), Vector4(1, 0, 0, 1), 1.0f);
        }

        // Throw
        void ThrowHeldItem(Vector3 aimDir) {
            GameObject* item = heldItem;
            DropHeldItem(); // drop constraint
			
            // add force
            float throwForce = 1.0f;
            if (item->GetName() == "Stone") {
				throwForce = 150.0f; // slightly throw fragile package
            }
            item->GetPhysicsObject()->ApplyLinearImpulse(aimDir * throwForce);
            if (item->GetName() == "FragilePackage") {
				fragilePackage->SetAttached(false);
                if (this->GetName() == "Player") {
                    fragilePackage->SetHeldByPlayer(-1);
                }
            }
        }

        // Set/Check hold item
        GameObject* GetHeldItem() const { return heldItem; }
        void SetHeldItemNetwork(GameObject* item) { heldItem = item; }

		// Draw grapple line
        void DrawGrappleLine() {
            if (heldItem) {
                Vector3 startPos = GetTransform().GetPosition() + Vector3(0, 1.5f, 0);
                Vector3 endPos = heldItem->GetTransform().GetPosition();

				// green line
                Debug::DrawLine(startPos, endPos, Vector4(0, 1, 0, 1));
            }
        }

    protected:
        // Create constraint
        void AttachItem(GameObject* item) {
            heldItem = item;
            // constraint£ºkeep 5.0f distance
            grabConstraint = new PositionConstraint(this, item, 5.0f);
            gameWorld->AddConstraint(grabConstraint);

			// replace WakeUp()£ºa small disturbance, make it more natural
            if (item->GetPhysicsObject()) {
                item->GetPhysicsObject()->ApplyAngularImpulse(Vector3(0.01f, 0.01f, 0.01f));
            }
        }

		// Delete constraint
        void DropHeldItem() {
            if (grabConstraint) {
                gameWorld->RemoveConstraint(grabConstraint, true);
                grabConstraint = nullptr;
            }
            heldItem = nullptr;
        }

        GameWorld* gameWorld;
        GameObject* heldItem;
        PositionConstraint* grabConstraint;
		FragileGameObject* fragilePackage;
		float actionCooldown = 0.0f; // cooldown time between actions
    };
}