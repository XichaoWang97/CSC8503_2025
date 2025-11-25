#include "FragileGameObject.h"
#include "PhysicsObject.h"
#include "RenderObject.h"
#include "Debug.h"
#include <iostream> // For debug output

using namespace NCL;
using namespace CSC8503;
using namespace Maths;

FragileGameObject::FragileGameObject(const std::string& name) : GameObject(name) {
    maxHealth = 100.0f;
    health = maxHealth;
    isBroken = false;
}

FragileGameObject::~FragileGameObject() {
}

void FragileGameObject::OnCollisionBegin(GameObject* otherObject) {
    if (isBroken) return; // 已经碎了就不再计算

    PhysicsObject* myPhys = GetPhysicsObject();
    if (!myPhys) return;

    // 获取对方的速度 (如果是静态物体，速度为0)
    Vector3 otherVel = Vector3(0, 0, 0);
    if (otherObject->GetPhysicsObject()) {
        otherVel = otherObject->GetPhysicsObject()->GetLinearVelocity();
    }

    // 计算相对速度
    Vector3 myVel = myPhys->GetLinearVelocity();
    Vector3 relVel = myVel - otherVel;
    float impactSpeed = Vector::Length(relVel);

    // 设定伤害阈值 (只有超过这个速度才算伤害)
    float damageThreshold = 10.0f;

    if (impactSpeed > damageThreshold) {
        // 伤害公式：速度越快，伤害越高
        float damage = (impactSpeed - damageThreshold) * 2.0f;
        health -= damage;

        std::cout << "Package Hit! Speed: " << impactSpeed << " Damage: " << damage << " HP: " << health << std::endl;

        if (health <= 0) {
            health = 0;
            isBroken = true;
            // 视觉反馈：变红表示破碎
            if (GetRenderObject()) {
                GetRenderObject()->SetColour(Vector4(1, 0, 0, 1));
            }
            Debug::Print("PACKAGE DESTROYED!", Vector2(30, 50), Vector4(1, 0, 0, 1));
        }
        else {
            // 视觉反馈：受伤变橙色，逐渐变深
            if (GetRenderObject()) {
                // 根据血量从 蓝色(100%) 渐变到 橙色(0%)
                float healthRatio = health / maxHealth;
                GetRenderObject()->SetColour(Vector4(1.0f - healthRatio, healthRatio * 0.5f, healthRatio, 1));
            }
        }
    }
}