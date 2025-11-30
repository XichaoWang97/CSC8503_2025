#include "MyGame.h"
#include "GameWorld.h"
#include "PhysicsSystem.h"
#include "PhysicsObject.h"
#include "RenderObject.h"
#include "TextureLoader.h"

#include "PositionConstraint.h"
#include "OrientationConstraint.h"
#include "StateGameObject.h"

#include "Window.h"
#include "Texture.h"
#include "Shader.h"
#include "Mesh.h"

#include "Debug.h"

#include "KeyboardMouseController.h"

#include "GameTechRendererInterface.h"

#include "Ray.h"
using namespace NCL::Maths::Vector;
using namespace NCL;
using namespace CSC8503;

MyGame::MyGame(GameWorld& inWorld, GameTechRendererInterface& inRenderer, PhysicsSystem& inPhysics)
	: world(inWorld),
	renderer(inRenderer),
	physics(inPhysics)
{

	forceMagnitude = 10.0f;
	useGravity = false;
	inSelectionMode = false;

	controller = new KeyboardMouseController(*Window::GetWindow()->GetKeyboard(), *Window::GetWindow()->GetMouse());

	world.GetMainCamera().SetController(*controller);

	world.SetSunPosition({ -200.0f, 60.0f, -200.0f });
	world.SetSunColour({ 0.8f, 0.8f, 0.5f });

	controller->MapAxis(0, "Sidestep");
	controller->MapAxis(1, "UpDown");
	controller->MapAxis(2, "Forward");

	controller->MapAxis(3, "XLook");
	controller->MapAxis(4, "YLook");

	cubeMesh = renderer.LoadMesh("cube.msh");
	sphereMesh = renderer.LoadMesh("sphere.msh");
	catMesh = renderer.LoadMesh("ORIGAMI_Chat.msh");
	kittenMesh = renderer.LoadMesh("Kitten.msh");

	enemyMesh = renderer.LoadMesh("Keeper.msh");
	bonusMesh = renderer.LoadMesh("19463_Kitten_Head_v1.msh");
	capsuleMesh = renderer.LoadMesh("capsule.msh");
	gooseMesh = renderer.LoadMesh("goose.msh");

	defaultTex = renderer.LoadTexture("Default.png");
	checkerTex = renderer.LoadTexture("checkerboard.png");
	glassTex = renderer.LoadTexture("stainedglass.tga");

	checkerMaterial.type = MaterialType::Opaque;
	checkerMaterial.diffuseTex = checkerTex;

	glassMaterial.type = MaterialType::Transparent;
	glassMaterial.diffuseTex = glassTex;

	InitCamera();
	InitWorld();
}

MyGame::~MyGame() {
	delete cubeMesh;
	delete sphereMesh;
	delete catMesh;
	delete kittenMesh;
	delete enemyMesh;
	delete bonusMesh;
	delete capsuleMesh;
	delete gooseMesh;

	delete defaultTex;
	delete checkerTex;
	delete glassTex;
	delete controller;

	delete navGrid;
}

void MyGame::UpdateGame(float dt) {
	// 1. 更新相机（处理鼠标旋转输入）
	// 注意：我们不再使用 LockedObjectMovement 里的强制锁定，而是用下面的 ARPG 逻辑
	if (!inSelectionMode) {
		world.GetMainCamera().UpdateCamera(dt);
	}

	// --- 第三人称跟随镜头 ---
	if (playerObject) {
		Vector3 playerPos = playerObject->GetTransform().GetPosition();

		// 获取当前相机的朝向（由鼠标控制的 Yaw 和 Pitch）
		float yaw = world.GetMainCamera().GetYaw();
		float pitch = world.GetMainCamera().GetPitch();

		// 限制 Pitch，防止相机钻地或翻转过头
		if (pitch > 30.0f) pitch = 30.0f;
		if (pitch < -60.0f) pitch = -60.0f;

		// 将欧拉角转换为四元数，计算相机的方向向量
		Quaternion cameraRot = Quaternion::EulerAnglesToQuaternion(pitch, yaw, 0);
		Vector3 cameraBackward = cameraRot * Vector3(0, 0, 1);
		//Vector3 cameraForward = cameraRot * Vector3(0, 0, -1);

		// 设定相机距离玩家的距离和高度偏移
		float dist = 15.0f;
		Vector3 offset = Vector3(0, 5, 0); // 稍微往上看一点

		// --- 相机避障逻辑 (Camera Collision) ---
		float maxDist = 15.0f; // 理想距离
		float currentDist = maxDist;
		// 从玩家头部向相机方向发射射线
		Vector3 rayOrigin = playerPos + offset;
		Vector3 rayDir = cameraBackward; // 已经归一化的方向
		Ray ray(rayOrigin, rayDir);
		RayCollision collision;
		// 如果射线在 maxDist 距离内碰到了东西（忽略玩家自己）
		if (world.Raycast(ray, collision, true, playerObject)) {
			if (collision.rayDistance < maxDist) {
				// 将距离设置为碰撞距离减去一点缓冲(0.5)，防止相机穿插在墙里面
				currentDist = collision.rayDistance - 0.5f;
				if (currentDist < 0.5f) currentDist = 0.5f; // 最小距离限制
			}
		}

		// 计算最终位置
		Vector3 cameraPos = rayOrigin + (cameraBackward * currentDist);
		world.GetMainCamera().SetPosition(cameraPos);
	}

	// --- 调试信息显示 --- 如果有终点，在此处画个标记或者文字
	if (targetZone) {
		Debug::Print("Destination", Vector2(80, 20), Debug::GREEN);
	}

	// --- 任务 1.4: 显示包裹血量 ---
	if (packageObject) {
		float health = packageObject->GetHealth();
		Vector4 col = (health > 50) ? Vector4(0, 1, 0, 1) : Vector4(1, 0, 0, 1);
		Debug::Print("Package HP: " + std::to_string((int)health), Vector2(5, 10), col);

		if (packageObject->IsBroken()) {
			Debug::Print("GAME OVER: PACKAGE BROKEN!", Vector2(30, 50), Vector4(1, 0, 0, 1));
		}
	}

	// --- 任务 1.5: 谜题逻辑 (压力板与门) ---
	if (puzzleDoor && pressurePlate) {
		bool isTriggered = false;
		Vector3 platePos = pressurePlate->GetTransform().GetPosition();
		// 压力板区域大小 (根据 InitCourierLevel 中设置的大小适当放宽)
		// 压力板尺寸是 5x0.5x5 (HalfDims), 我们在 Y 轴上放宽一点以便检测站在上面的物体
		Vector3 triggerSize = Vector3(5.5f, 2.0f, 5.5f);

		// 简单的 AABB 包含检测 lambda
		auto CheckTrigger = [&](GameObject* obj) {
			if (!obj) return false;
			Vector3 pos = obj->GetTransform().GetPosition();
			// 检查 obj 是否在压力板的 AABB 范围内
			return (pos.x > platePos.x - triggerSize.x && pos.x < platePos.x + triggerSize.x &&
				pos.y > platePos.y - triggerSize.y && pos.y < platePos.y + triggerSize.y &&
				pos.z > platePos.z - triggerSize.z && pos.z < platePos.z + triggerSize.z);
			};

		// 只要玩家或者包裹在上面，就算触发
		if (CheckTrigger(playerObject) || CheckTrigger(packageObject)) {
			isTriggered = true;
		}

		// --- 门的动作 ---
		Vector3 doorPos = puzzleDoor->GetTransform().GetPosition();
		// 门初始在 Y=-10 (地面上), 打开时移动到 Y=-30 (沉入地下)
		float targetY = isTriggered ? -30.0f : -10.0f;
		float doorSpeed = 20.0f;

		if (abs(doorPos.y - targetY) > 0.01f) {
			if (doorPos.y > targetY) {
				doorPos.y -= doorSpeed * dt;
				if (doorPos.y < targetY) doorPos.y = targetY;
			}
			else {
				doorPos.y += doorSpeed * dt;
				if (doorPos.y > targetY) doorPos.y = targetY;
			}
			puzzleDoor->GetTransform().SetPosition(doorPos);
		}

		// --- 视觉反馈 ---
		if (isTriggered) {
			pressurePlate->GetRenderObject()->SetColour(Vector4(0, 1, 0, 1)); // 激活变绿
			Debug::Print("Gate Open!", Vector2(45, 80), Debug::GREEN);
		}
		else {
			pressurePlate->GetRenderObject()->SetColour(Vector4(1, 1, 0, 1)); // 默认黄色
			Debug::Print("Gate Closed - Find the Plate", Vector2(30, 80), Debug::YELLOW);
		}
	}

	// --- 任务 0.3: 玩家控制 ---
	// 只有当并不是在自由视角模式(SelectionMode)下，且玩家存在时才允许控制
	if (!inSelectionMode && playerObject) {
		PlayerControl(dt);
		// 显示操作提示
		Debug::Print("WASD: Move", Vector2(5, 80), Debug::WHITE);
		Debug::Print("SPACE: Jump", Vector2(5, 85), Debug::WHITE);
	}

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::F1)) {
		InitWorld(); //We can reset the simulation at any time with F1
		selectionObject = nullptr;
	}

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::F2)) {
		InitCamera(); //F2 will reset the camera to a specific default place
	}

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::G)) {
		useGravity = !useGravity; //Toggle gravity!
		physics.UseGravity(useGravity);
	}
	//Running certain physics updates in a consistent order might cause some
	//bias in the calculations - the same objects might keep 'winning' the constraint
	//allowing the other one to stretch too much etc. Shuffling the order so that it
	//is random every frame can help reduce such bias.
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::F9)) {
		world.ShuffleConstraints(true);
	}
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::F10)) {
		world.ShuffleConstraints(false);
	}

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::F7)) {
		world.ShuffleObjects(true);
	}
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::F8)) {
		world.ShuffleObjects(false);
	}

	// 自由视角模式下的调试移动
	if (inSelectionMode && selectionObject) {
		DebugObjectMovement();
	}

	RayCollision closestCollision;
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::K) && selectionObject) {
		Vector3 rayPos;
		Vector3 rayDir;

		rayDir = selectionObject->GetTransform().GetOrientation() * Vector3(0, 0, -1);
		rayPos = selectionObject->GetTransform().GetPosition();

		Ray r = Ray(rayPos, rayDir);

		if (world.Raycast(r, closestCollision, true, selectionObject)) {
			if (objClosest) {
				objClosest->GetRenderObject()->SetColour(Vector4(1, 1, 1, 1));
			}
			objClosest = (GameObject*)closestCollision.node;

			objClosest->GetRenderObject()->SetColour(Vector4(1, 0, 1, 1));
		}
	}

	//This year we can draw debug textures as well!
	Debug::DrawTex(*defaultTex, Vector2(10, 10), Vector2(5, 5), Debug::WHITE);
	Debug::DrawLine(Vector3(), Vector3(0, 100, 0), Vector4(1, 0, 0, 1));
	if (useGravity) {
		Debug::Print("(G)ravity on", Vector2(5, 95), Debug::RED);
	}
	else {
		Debug::Print("(G)ravity off", Vector2(5, 95), Debug::RED);
	}

	SelectObject();
	MoveSelectedObject();

	world.OperateOnContents(
		[dt](GameObject* o) {
			o->Update(dt);
		}
	);
}

void MyGame::InitCamera() {
	world.GetMainCamera().SetNearPlane(0.1f);
	world.GetMainCamera().SetFarPlane(500.0f);
	world.GetMainCamera().SetPitch(-15.0f);
	world.GetMainCamera().SetYaw(315.0f);
	world.GetMainCamera().SetPosition(Vector3(-60, 40, 60));
	lockedObject = nullptr;
}

void MyGame::InitWorld() {
	world.ClearAndErase();
	physics.Clear();

	InitCourierLevel();
}

GameObject* MyGame::AddFloorToWorld(const Vector3& position) {
	GameObject* floor = new GameObject("floor");

	Vector3 floorSize = Vector3(200, 2, 200);
	AABBVolume* volume = new AABBVolume(floorSize);
	floor->SetBoundingVolume(volume);
	floor->GetTransform()
		.SetScale(floorSize * 2.0f)
		.SetPosition(position);

	floor->SetRenderObject(new RenderObject(floor->GetTransform(), cubeMesh, checkerMaterial));
	floor->SetPhysicsObject(new PhysicsObject(floor->GetTransform(), floor->GetBoundingVolume()));

	floor->GetPhysicsObject()->SetInverseMass(0);
	floor->GetPhysicsObject()->InitCubeInertia();

	world.AddGameObject(floor);

	return floor;
}

GameObject* MyGame::AddSphereToWorld(const Vector3& position, float radius, float inverseMass) {
	GameObject* sphere = new GameObject("sphere");

	Vector3 sphereSize = Vector3(radius, radius, radius);
	SphereVolume* volume = new SphereVolume(radius);
	sphere->SetBoundingVolume(volume);

	sphere->GetTransform()
		.SetScale(sphereSize)
		.SetPosition(position);

	sphere->SetRenderObject(new RenderObject(sphere->GetTransform(), sphereMesh, checkerMaterial));
	sphere->SetPhysicsObject(new PhysicsObject(sphere->GetTransform(), sphere->GetBoundingVolume()));

	sphere->GetPhysicsObject()->SetInverseMass(inverseMass);
	sphere->GetPhysicsObject()->InitSphereInertia();

	world.AddGameObject(sphere);

	return sphere;
}

GameObject* MyGame::AddCubeToWorld(const Vector3& position, Vector3 dimensions, float inverseMass) {
	GameObject* cube = new GameObject("cube");

	AABBVolume* volume = new AABBVolume(dimensions);
	cube->SetBoundingVolume(volume);

	cube->GetTransform()
		.SetPosition(position)
		.SetScale(dimensions * 2.0f);

	cube->SetRenderObject(new RenderObject(cube->GetTransform(), cubeMesh, checkerMaterial));
	cube->SetPhysicsObject(new PhysicsObject(cube->GetTransform(), cube->GetBoundingVolume()));

	cube->GetPhysicsObject()->SetInverseMass(inverseMass);
	cube->GetPhysicsObject()->InitCubeInertia();

	world.AddGameObject(cube);

	return cube;
}

GameObject* MyGame::AddPlayerToWorld(const Vector3& position) {
	float meshSize = 1.0f;
	float inverseMass = 0.5f;

	GameObject* character = new GameObject("player");
	SphereVolume* volume = new SphereVolume(0.5f);

	character->SetBoundingVolume(volume);

	character->GetTransform()
		.SetScale(Vector3(meshSize, meshSize, meshSize))
		.SetPosition(position);

	character->SetRenderObject(new RenderObject(character->GetTransform(), catMesh, notexMaterial));
	character->GetRenderObject()->SetColour(Vector4(0, 1, 1, 1)); // 给玩家设置特殊颜色 (例如青色 Cyan)

	PhysicsObject* physicsObj = new PhysicsObject(character->GetTransform(), character->GetBoundingVolume());
	physicsObj->SetInverseMass(inverseMass);
	physicsObj->InitSphereInertia();

	// --- 改动 3: 取消弹跳 ---
	physicsObj->SetElasticity(0.0f); // 设为 0，落地不反弹
	// physicsObj->SetFriction(0.8f); // 可以增加摩擦力，但这通常用于物体之间的接触计算，这里主要靠我们手写的刹车逻辑

	character->SetPhysicsObject(physicsObj);

	world.AddGameObject(character);

	return character;
}

RivalAI* MyGame::AddRivalAIToWorld(const Vector3& position) {
	float meshSize = 1.0f;
	float inverseMass = 0.5f;

	RivalAI* rival = new RivalAI(navGrid);
	SphereVolume* volume = new SphereVolume(0.5f);

	rival->SetBoundingVolume(volume);

	rival->GetTransform()
		.SetScale(Vector3(meshSize, meshSize, meshSize))
		.SetPosition(position);

	rival->SetRenderObject(new RenderObject(rival->GetTransform(), catMesh, notexMaterial));
	rival->GetRenderObject()->SetColour(Vector4(0.5f, 0, 0.5f, 1)); // 给玩家设置特殊颜色 (例如青色 Cyan)

	PhysicsObject* physicsObj = new PhysicsObject(rival->GetTransform(), rival->GetBoundingVolume());
	physicsObj->SetInverseMass(inverseMass);
	physicsObj->InitSphereInertia();

	// [关键] 注入 GameWorld，否则它找不到包裹也不能生成石头
	rival->SetGameWorld(&world);
	// --- 改动 3: 取消弹跳 ---
	physicsObj->SetElasticity(0.0f); // 设为 0，落地不反弹
	// physicsObj->SetFriction(0.8f); // 可以增加摩擦力，但这通常用于物体之间的接触计算，这里主要靠我们手写的刹车逻辑

	rival->SetPhysicsObject(physicsObj);

	world.AddGameObject(rival);

	return rival;
}

StateGameObject* MyGame::AddEnemyToWorld(const Vector3& position) {
	float meshSize = 3.0f;
	float inverseMass = 0.5f;

	StateGameObject* character = new StateGameObject("enemy");

	AABBVolume* volume = new AABBVolume(Vector3(0.3f, 0.9f, 0.3f) * meshSize);
	character->SetBoundingVolume(volume);

	character->GetTransform()
		.SetScale(Vector3(meshSize, meshSize, meshSize))
		.SetPosition(position);

	character->SetRenderObject(new RenderObject(character->GetTransform(), enemyMesh, notexMaterial));
	character->GetRenderObject()->SetColour(Vector4(1, 0, 0, 1)); // red color ENEMY

	PhysicsObject* physicsObj = new PhysicsObject(character->GetTransform(), character->GetBoundingVolume());

	physicsObj->SetInverseMass(inverseMass);
	physicsObj->InitSphereInertia();

	character->SetPhysicsObject(physicsObj);

	world.AddGameObject(character);

	return character;
}

GooseNPC* MyGame::AddGooseNPCToWorld(const Vector3& position)
{
	float meshSize = 3.0f;
	float inverseMass = 0.5f;

	GooseNPC* goose = new GooseNPC(navGrid, playerObject); // init goose with navgrid and player reference
	AABBVolume* volume = new AABBVolume(Vector3(0.3f, 0.9f, 0.3f) * meshSize);
	goose->SetBoundingVolume(volume);

	goose->GetTransform()
		.SetScale(Vector3(meshSize, meshSize, meshSize))
		.SetPosition(position);

	goose->SetRenderObject(new RenderObject(goose->GetTransform(), gooseMesh, notexMaterial));
	goose->GetRenderObject()->SetColour(Vector4(1, 0.5f, 0, 1)); // orange color goose

	PhysicsObject* physicsObj = new PhysicsObject(goose->GetTransform(), goose->GetBoundingVolume());

	physicsObj->SetInverseMass(inverseMass);
	physicsObj->InitSphereInertia();

	goose->SetPhysicsObject(physicsObj);

	world.AddGameObject(goose);

	return goose;
}

bool MyGame::SelectObject() {
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::Q)) {
		inSelectionMode = !inSelectionMode;
		if (inSelectionMode) {
			Window::GetWindow()->ShowOSPointer(true);
			Window::GetWindow()->LockMouseToWindow(false);
		}
		else {
			Window::GetWindow()->ShowOSPointer(false);
			Window::GetWindow()->LockMouseToWindow(true);
		}
	}
	if (inSelectionMode) {
		Debug::Print("Press Q to change to camera mode!", Vector2(5, 85));

		if (Window::GetMouse()->ButtonDown(NCL::MouseButtons::Left)) {
			if (selectionObject) {	//set colour to deselected;
				selectionObject->GetRenderObject()->SetColour(Vector4(1, 1, 1, 1));
				selectionObject = nullptr;
			}

			Ray ray = CollisionDetection::BuildRayFromMouse(world.GetMainCamera());

			RayCollision closestCollision;
			if (world.Raycast(ray, closestCollision, true)) {
				selectionObject = (GameObject*)closestCollision.node;

				selectionObject->GetRenderObject()->SetColour(Vector4(0, 1, 0, 1));
				return true;
			}
			else {
				return false;
			}
		}
		if (Window::GetKeyboard()->KeyPressed(NCL::KeyCodes::L)) {
			if (selectionObject) {
				if (lockedObject == selectionObject) {
					lockedObject = nullptr;
				}
				else {
					lockedObject = selectionObject;
				}
			}
		}
	}
	else {
		Debug::Print("Press Q to change to select mode!", Vector2(5, 85));
	}
	return false;
}

void MyGame::MoveSelectedObject() {
	Debug::Print("Click Force:" + std::to_string(forceMagnitude), Vector2(5, 90));
	forceMagnitude += Window::GetMouse()->GetWheelMovement() * 100.0f;

	if (!selectionObject) {
		return;//we haven't selected anything!
	}
	//Push the selected object!
	if (Window::GetMouse()->ButtonPressed(NCL::MouseButtons::Right)) {
		Ray ray = CollisionDetection::BuildRayFromMouse(world.GetMainCamera());

		RayCollision closestCollision;
		if (world.Raycast(ray, closestCollision, true)) {
			if (closestCollision.node == selectionObject) {
				selectionObject->GetPhysicsObject()->AddForceAtPosition(ray.GetDirection() * forceMagnitude, closestCollision.collidedAt);
			}
		}
	}
}

void MyGame::LockedObjectMovement() {
	Matrix4 view = world.GetMainCamera().BuildViewMatrix();
	Matrix4 camWorld = Matrix::Inverse(view);

	Vector3 rightAxis = Vector3(camWorld.GetColumn(0)); //view is inverse of model!

	//forward is more tricky -  camera forward is 'into' the screen...
	//so we can take a guess, and use the cross of straight up, and
	//the right axis, to hopefully get a vector that's good enough!

	Vector3 fwdAxis = Vector::Cross(Vector3(0, 1, 0), rightAxis);
	fwdAxis.y = 0.0f;
	fwdAxis = Vector::Normalise(fwdAxis);

	if (Window::GetKeyboard()->KeyDown(KeyCodes::UP)) {
		selectionObject->GetPhysicsObject()->AddForce(fwdAxis);
	}

	if (Window::GetKeyboard()->KeyDown(KeyCodes::DOWN)) {
		selectionObject->GetPhysicsObject()->AddForce(-fwdAxis);
	}

	if (Window::GetKeyboard()->KeyDown(KeyCodes::NEXT)) {
		selectionObject->GetPhysicsObject()->AddForce(Vector3(0, -10, 0));
	}
}

void MyGame::DebugObjectMovement() {
	//If we've selected an object, we can manipulate it with some key presses
	if (inSelectionMode && selectionObject) {
		//Twist the selected object!
		if (Window::GetKeyboard()->KeyDown(KeyCodes::LEFT)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(-10, 0, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::RIGHT)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(10, 0, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::NUM7)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(0, 10, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::NUM8)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(0, -10, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::RIGHT)) {
			selectionObject->GetPhysicsObject()->AddTorque(Vector3(10, 0, 0));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::UP)) {
			selectionObject->GetPhysicsObject()->AddForce(Vector3(0, 0, -10));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::DOWN)) {
			selectionObject->GetPhysicsObject()->AddForce(Vector3(0, 0, 10));
		}

		if (Window::GetKeyboard()->KeyDown(KeyCodes::NUM5)) {
			selectionObject->GetPhysicsObject()->AddForce(Vector3(0, -10, 0));
		}
	}
}

void MyGame::BridgeConstraintTest() {
	Vector3 cubeSize = Vector3(8, 8, 8);

	float invCubeMass = 5; // how heavy the middle pieces are
	int numLinks = 10;
	float maxDistance = 30; // constraint distance
	float cubeDistance = 20; // distance between links

	Vector3 startPos = Vector3(500, 500, 500);

	GameObject* start = AddCubeToWorld(startPos + Vector3(0, 0, 0), cubeSize, 0);
	GameObject* end = AddCubeToWorld(startPos + Vector3((numLinks + 2) * cubeDistance, 0, 0), cubeSize, 0);

	GameObject* previous = start;

	for (int i = 0; i < numLinks; ++i) {
		GameObject* block = AddCubeToWorld(startPos + Vector3((i + 1) * cubeDistance, 0, 0), cubeSize, invCubeMass);
		PositionConstraint* constraint = new PositionConstraint(previous, block, maxDistance);
		world.AddConstraint(constraint);
		previous = block;
	}

	PositionConstraint* constraint = new PositionConstraint(previous, end, maxDistance);
	world.AddConstraint(constraint);
}

// --- 关卡搭建核心逻辑 ---
void MyGame::InitCourierLevel() {
	// 1. 添加地板
	// 位置在 Y=-20，大小足够大
	AddFloorToWorld(Vector3(0, -20, 0));

	// 2. 构建边界墙壁 (Static Cubes, Mass = 0)
	// 假设游戏区域大概是 200x200 
	float wallHeight = 20.0f;
	float boundarySize = 100.0f;
	float wallThickness = 2.0f;

	// 北南西东4墙
	AddCubeToWorld(Vector3(0, -20 + wallHeight, -boundarySize), Vector3(boundarySize, wallHeight, wallThickness), 0.0f);
	AddCubeToWorld(Vector3(0, -20 + wallHeight, boundarySize), Vector3(boundarySize, wallHeight, wallThickness), 0.0f);
	AddCubeToWorld(Vector3(-boundarySize, -20 + wallHeight, 0), Vector3(wallThickness, wallHeight, boundarySize), 0.0f);
	AddCubeToWorld(Vector3(boundarySize, -20 + wallHeight, 0), Vector3(wallThickness, wallHeight, boundarySize), 0.0f);

	// 3. 放置内部障碍物 (构成简单的迷宫或路径)
	// 示例：左侧的一个长条障碍
	AddCubeToWorld(Vector3(-30, -10, 0), Vector3(2, 10, 40), 0.0f);
	// 示例：右侧的一个平台
	AddCubeToWorld(Vector3(40, -15, 20), Vector3(10, 5, 10), 0.0f);

	// 4. 放置终点区域 (Delivery Zone)
	// 使用绿色的半透明方块表示，无碰撞或设为 Trigger (这里先设为普通静态物体，视觉上区分)
	Vector3 targetPos = Vector3(60, -18, -60);
	targetZone = AddCubeToWorld(targetPos, Vector3(10, 1, 10), 0.0f);
	if (targetZone && targetZone->GetRenderObject()) {
		targetZone->GetRenderObject()->SetColour(Vector4(0, 1, 0, 0.5f)); // 绿色半透明
	}

	// 5. 谜题元素：门和压力板
	// 压力板 (黄色)
	pressurePlate = AddCubeToWorld(Vector3(0, -18, 40), Vector3(5, 0.5f, 5), 0.0f);
	if (pressurePlate && pressurePlate->GetRenderObject()) {
		pressurePlate->GetRenderObject()->SetColour(Vector4(1, 1, 0, 1)); // 黄色
	}

	// 门 (蓝色，阻挡路径)
	// 放置在通往终点的必经之路上
	puzzleDoor = AddCubeToWorld(Vector3(60, -10, -30), Vector3(15, 10, 2), 0.0f);
	if (puzzleDoor && puzzleDoor->GetRenderObject()) {
		puzzleDoor->GetRenderObject()->SetColour(Vector4(0, 0, 1, 1)); // 蓝色
	}

	// --- 任务 0.3 修改: 将返回值存入 playerObject ---
	Vector3 playerStartPos = Vector3(-60, 5, 60);
	playerObject = AddPlayerToWorld(playerStartPos);
	// --- 任务 1.4: 使用 FragileGameObject 创建包裹 ---
	packageObject = AddFragilePackageToWorld(Vector3(-50, 2, 60)); // 放低一点，防止落地直接摔坏

	// --- Enemy 简单 AI 设置 ---
	Vector3 enemyStartPos = Vector3(0, 0, 0);
	std::vector<Vector3> aiPath;
	float pathY = -16.0f;
	aiPath.push_back(Vector3(20, pathY, 20));
	aiPath.push_back(Vector3(20, pathY, -20));
	aiPath.push_back(Vector3(-20, pathY, -20));
	aiPath.push_back(Vector3(-20, pathY, 20));

	// Add Enemy (RED) that patrols and targets the player
	StateGameObject* enemy = AddEnemyToWorld(enemyStartPos);
	enemy->SetPatrolPath(aiPath);
	enemy->SetTarget(playerObject);
	enemy->SetGameWorld(&world);
	enemy->SetResetPoint(playerStartPos);

	// Init Navigation Grid
	if (!navGrid) {
		navGrid = new NavigationGrid("TestGrid1.txt");
	}
	// Add Goose NPC and rival AI
	gooseNPC = AddGooseNPCToWorld(Vector3(-20, 0, -20));
	rivalAI = AddRivalAIToWorld(Vector3(20, 0, 20));

}

// --- 现代 ARPG 控制实现 ---
void MyGame::PlayerControl(float dt) {
	if (!playerObject) return;

	PhysicsObject* phys = playerObject->GetPhysicsObject();
	if (!phys) return;

	Transform& transform = playerObject->GetTransform();

	float forceMagnitude = 60.0f;
	float maxSpeed = 15.0f;
	float rotationSpeed = 10.0f; // 转向速度 (Lerp factor)

	// 1. 获取相机的视角 (只取 Yaw)
	float yaw = world.GetMainCamera().GetYaw();
	Quaternion cameraRot = Quaternion::EulerAnglesToQuaternion(0, yaw, 0);

	// 2. 构建本地输入向量 (Local Input)
	// W = 前 (0,0,-1), S = 后 (0,0,1), A = 左 (-1,0,0), D = 右 (1,0,0)
	Vector3 inputDir(0, 0, 0);
	if (Window::GetKeyboard()->KeyDown(KeyCodes::W)) inputDir.z -= 1;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::S)) inputDir.z += 1;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::A)) inputDir.x -= 1;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::D)) inputDir.x += 1;

	bool isMoving = (Vector::LengthSquared(inputDir) > 0);

	if (isMoving) {
		// 归一化输入，防止斜向移动速度变快
		inputDir = Vector::Normalise(inputDir);

		// 3. 计算世界空间的目标移动方向
		// 将本地输入 (相对于相机) 转换为世界方向
		Vector3 targetDir = cameraRot * inputDir;

		// 4. 自动转向 (Auto-Rotate)
		// 计算目标朝向的四元数, 创建一个从 (0,0,0) 看向 targetDir 的旋转
		Matrix4 lookAtMat = Matrix::View(Vector3(0, 0, 0), -targetDir, Vector3(0, 1, 0));
		Quaternion targetOrientation(Matrix::Inverse(lookAtMat));

		// 使用 Slerp 平滑插值当前朝向到目标朝向
		Quaternion currentOrientation = transform.GetOrientation();
		// --- 修复: 四元数最短路径检查 (Shortest Path Check) ---
		// 如果两个四元数的点积 < 0，说明它们夹角 > 90度，插值会走长路径（例如 270 度）。
		// 将其中一个四元数取反，可以保证走短路径（90 度）。
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

		// 5.Apply Force
		phys->AddForce(targetDir * forceMagnitude);
	}

	// 6. 速度限制 (Speed Limit)
	Vector3 velocity = phys->GetLinearVelocity();
	Vector3 planarVelocity(velocity.x, 0, velocity.z);

	if (Vector::Length(planarVelocity) > maxSpeed) {
		planarVelocity = Vector::Normalise(planarVelocity) * maxSpeed;
		phys->SetLinearVelocity(Vector3(planarVelocity.x, velocity.y, planarVelocity.z));
	}

	// 7. 刹车逻辑 (Damping)
	if (!isMoving) {
		if (Vector::Length(planarVelocity) > 0.1f) {
			float dampingFactor = 5.0f;
			phys->AddForce(-planarVelocity * dampingFactor);
		}
	}

	// 8. 跳跃
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::SPACE)) {
		if (IsPlayerOnGround(playerObject)) {
			phys->ApplyLinearImpulse(Vector3(0, 20, 0));
		}
	}
}

// --- 使用射线检测地面 ---
bool MyGame::IsPlayerOnGround(GameObject* obj) {
	if (!obj) return false;

	// 获取玩家位置
	Vector3 playerPos = obj->GetTransform().GetPosition();

	// 向下发射射线
	Ray ray(playerPos, Vector3(0, -1, 0));
	RayCollision collision;

	// 玩家半径是 1.0 (在 AddPlayerToWorld 里定义的)
	// 我们检测距离稍微大一点点 (1.1)，允许轻微的误差或斜坡检测
	float groundCheckDist = 1.1f;

	// 使用 Raycast，记得传入 playerObject 以忽略自身
	if (world.Raycast(ray, collision, true, obj)) {
		if (collision.rayDistance < groundCheckDist) {
			return true; // 碰到了地面或物体
		}
	}

	return false;
}

// --- 任务 1.4: 实现包裹创建函数 ---
FragileGameObject* MyGame::AddFragilePackageToWorld(const Vector3& position) {
	FragileGameObject* package = new FragileGameObject("FragilePackage");

	SphereVolume* volume = new SphereVolume(1.0f);
	package->SetBoundingVolume(volume);
	package->GetTransform().SetScale(Vector3(1, 1, 1)).SetPosition(position);

	package->SetRenderObject(new RenderObject(package->GetTransform(), bonusMesh, glassMaterial));
	package->GetRenderObject()->SetColour(Vector4(0, 0, 1, 1)); // 初始蓝色 (满血)

	PhysicsObject* physicsObj = new PhysicsObject(package->GetTransform(), package->GetBoundingVolume());
	physicsObj->SetInverseMass(1.0f);
	physicsObj->InitSphereInertia();
	physicsObj->SetElasticity(0.3f); // 低弹性，防止弹得太厉害

	package->SetPhysicsObject(physicsObj);

	world.AddGameObject(package);

	return package;
}
