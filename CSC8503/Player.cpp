#include "Player.h"
#include "GameWorld.h"
#include "PhysicsSystem.h"
#include "PhysicsObject.h"
using namespace NCL;
using namespace CSC8503;

Player::Player(GameWorld* world) : GameCharacter("Player", world) {
}

Player::~Player() {
}

void Player::Update(float dt) {
    if (actionCooldown > 0.0f) {
        actionCooldown -= dt;
    }
	PlayerControl(dt);
    DrawGrappleLine();
}

// Player control function, modern Engine style
void Player::PlayerControl(float dt) {
	PhysicsObject* phys = GetPhysicsObject(); // Get self PhysicsObject
    if (!phys) return;

    Transform& transform = GetTransform(); // Get self Transform

    float forceMagnitude = 60.0f;
    float maxSpeed = 15.0f;
    float rotationSpeed = 10.0f;

    // 1. 获取相机的视角
    float yaw = gameWorld->GetMainCamera().GetYaw();
    Quaternion cameraRot = Quaternion::EulerAnglesToQuaternion(0, yaw, 0);

    // 2. 构建本地输入向量
    Vector3 inputDir(0, 0, 0);
    if (Window::GetKeyboard()->KeyDown(KeyCodes::W)) inputDir.z -= 1;
    if (Window::GetKeyboard()->KeyDown(KeyCodes::S)) inputDir.z += 1;
    if (Window::GetKeyboard()->KeyDown(KeyCodes::A)) inputDir.x -= 1;
    if (Window::GetKeyboard()->KeyDown(KeyCodes::D)) inputDir.x += 1;

    bool isMoving = (Vector::LengthSquared(inputDir) > 0);

    if (isMoving) {
        inputDir = Vector::Normalise(inputDir);
        Vector3 targetDir = cameraRot * inputDir;

        // 4. 自动转向
        Matrix4 lookAtMat = Matrix::View(Vector3(0, 0, 0), -targetDir, Vector3(0, 1, 0));
        Quaternion targetOrientation(Matrix::Inverse(lookAtMat));

        Quaternion currentOrientation = transform.GetOrientation();

        // 四元数最短路径修正
        float dot = Quaternion::Dot(currentOrientation, targetOrientation);
        if (dot < 0.0f) {
            targetOrientation.x = -targetOrientation.x;
            targetOrientation.y = -targetOrientation.y;
            targetOrientation.z = -targetOrientation.z;
            targetOrientation.w = -targetOrientation.w;
        }

        Quaternion newOrientation = Quaternion::Slerp(currentOrientation, targetOrientation, rotationSpeed * dt);

        transform.SetOrientation(newOrientation);
        phys->SetAngularVelocity(Vector3(0, 0, 0));

        // Apply Force
        phys->AddForce(targetDir * forceMagnitude);
    }

    // Grab and Throw
    if (Window::GetMouse()->ButtonPressed(MouseButtons::Left) && actionCooldown <= 0.0f) {
        Vector3 playerDir = GetTransform().GetOrientation() * Vector3(0, 0, 1);

        if (GetHeldItem()) {
            ThrowHeldItem(playerDir); // 有东西就扔
            actionCooldown = 0.5f;
        }
        else {
            TryGrab(playerDir);       // 没东西就抓d
            actionCooldown = 0.5f;
        }
    }

    // 6. 速度限制
    Vector3 velocity = phys->GetLinearVelocity();
    Vector3 planarVelocity(velocity.x, 0, velocity.z);

    if (Vector::Length(planarVelocity) > maxSpeed) {
        planarVelocity = Vector::Normalise(planarVelocity) * maxSpeed;
        phys->SetLinearVelocity(Vector3(planarVelocity.x, velocity.y, planarVelocity.z));
    }

    // 7. 刹车逻辑
    if (!isMoving) {
        if (Vector::Length(planarVelocity) > 0.1f) {
            float dampingFactor = 5.0f;
            phys->AddForce(-planarVelocity * dampingFactor);
        }
    }

    // 8. 跳跃
    if (Window::GetKeyboard()->KeyPressed(KeyCodes::SPACE)) {
        if (IsPlayerOnGround()) { // 调用不带参数的版本
            phys->ApplyLinearImpulse(Vector3(0, 15, 0));
        }
    }
}

// Test if player is on ground
bool Player::IsPlayerOnGround() {

    Vector3 playerPos = GetTransform().GetPosition();

	// emit a ray downwards
    Ray ray(playerPos, Vector3(0, -1, 0));
    RayCollision collision;

    float groundCheckDist = 1.1f;

    if (gameWorld->Raycast(ray, collision, true, this)) {
        if (collision.rayDistance < groundCheckDist) {
            return true;
        }
    }

    return false;
}