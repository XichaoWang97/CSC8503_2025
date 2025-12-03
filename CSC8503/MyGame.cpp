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
	coinMesh = renderer.LoadMesh("coin.msh");
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

	// Update camera
	if (!inSelectionMode) {
		world.GetMainCamera().UpdateCamera(dt);
	}

	// 第三人称跟随镜头
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

		// Avoid Camera Collision
		float maxDist = 15.0f;
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

	// Package health display
	if (packageObject) {
		float health = packageObject->GetHealth();
		Vector4 col = (health > 50) ? Vector4(0, 1, 0, 1) : Vector4(1, 0, 0, 1);
		Debug::Print("Package HP: " + std::to_string((int)health), Vector2(70, 90), col);

		// if package is broken
		if (packageObject->IsBroken()) {
			Debug::Print("PACKAGE BROKEN!", Vector2(30, 50), Vector4(1, 0, 0, 1));
			// drop package if held
			if (playerObject->GetHeldItem() != nullptr) {
				if (playerObject->GetHeldItem()->GetName() == "FragilePackage") {
					playerObject->ThrowHeldItem(Vector3(0, 0, 0));
				}
			}
			if (rivalAI->GetHeldItem() != nullptr) {
				if (rivalAI->GetHeldItem()->GetName() == "FragilePackage") {
					rivalAI->ThrowHeldItem(Vector3(0, 0, 0));
				}
			}
			packageObject->GetTransform().SetPosition(Vector3(0, -9999, 0)); // if destroyed, move it underground
			// reset function is in FragileGameObject.cpp
		}
	}

	// puzzle door and pressure plate logic
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
		if (CheckTrigger(playerObject) || CheckTrigger(cubeStone)) {
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

		if (isTriggered) {
			pressurePlate->GetRenderObject()->SetColour(Vector4(0, 1, 0, 1)); // 激活变绿
		}
		else {
			pressurePlate->GetRenderObject()->SetColour(Vector4(1, 1, 0, 1)); // 默认黄色
		}
	}

	// Coin Collection, iterate in reverse order
	for (int i = coins.size() - 1; i >= 0; --i) {
		GameObject* c = coins[i];
		if (!c->IsActive()) continue;

		// calculate distance to player
		float dist = Vector::Length((playerObject->GetTransform().GetPosition() - c->GetTransform().GetPosition()));
		GameObject* playerHeld = playerObject->GetHeldItem();
		// collection radius 2.5f, need FragilePackage to collect
		if (playerHeld) {
			if (dist < 2.5f && playerHeld->GetName() == "FragilePackage") {
				world.RemoveGameObject(c, true); // remove coin from world
				score++;
				coins.erase(coins.begin() + i); // remove coin from vector
			}
		}
	}

	// Win Condition
	if (!isGameWon && targetZone) {
		Vector3 zonePos = targetZone->GetTransform().GetPosition();
		Vector3 zoneSize = Vector3(10, 1, 10); // size of the target zone

		Vector3 pPos = playerObject->GetTransform().GetPosition();

		bool inZone = (pPos.x > zonePos.x - zoneSize.x && pPos.x < zonePos.x + zoneSize.x &&
			pPos.z > zonePos.z - zoneSize.z && pPos.z < zonePos.z + zoneSize.z);

		// score && inZone -> win
		if (inZone && score >= winningScore) {
			isGameWon = true;
		}
		// score is not enough
		if (inZone && score < winningScore) {
			Debug::Print("Need more coins!", Vector2(40, 40), Vector4(1, 0, 0, 1));
		}
	}
	// Death Condition - hit by goose
	if (!isGameOver && !isGameWon && playerObject && gooseNPC) {
		Vector3 pPos = playerObject->GetTransform().GetPosition();
		Vector3 gPos = gooseNPC->GetTransform().GetPosition();

		// 简单的距离检测：如果距离小于 2.0 米，视为被撞
		float dist = Vector::Length((pPos - gPos));
		if (dist < 2.0f) {
			isGameOver = true;
		}
	}

	// 游戏进行中的 HUD
	std::string scoreText = "Coins: " + std::to_string(score) + " / " + std::to_string(winningScore);
	// 根据是否收集满显示不同颜色
	Vector4 scoreColor = (score >= winningScore) ? Vector4(0, 1, 0, 1) : Vector4(1, 1, 0, 1);
	Debug::Print(scoreText, Vector2(75, 10), scoreColor); // 左上角显示
	//----------------------------------------------------------------------------------
	
	// player object update
	if (!inSelectionMode && playerObject) {
		playerObject->Update(dt);
	}
	// package object update
	if(packageObject) {
		packageObject->Update(dt);
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

	// Gravity status display
	if (useGravity) {
		Debug::Print("(G)ravity on", Vector2(5, 95), Debug::RED);
	}
	else {
		Debug::Print("(G)ravity off", Vector2(5, 95), Debug::RED);
	}

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
}

void MyGame::InitWorld() {
	world.ClearAndErase();
	physics.Clear();
	// 好像无效了？
	score = 0;
	isGameOver = false;
	isGameWon = false;

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
	GameObject* sphere = new GameObject("Stone"); // changed name to Stone

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

GameObject* MyGame::AddCubeToWorld(const Vector3& position, Vector3 dimensions, float inverseMass, std::string name) {
	GameObject* cube = new GameObject(name);

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

GameObject* MyGame::AddCoinToWorld(const Vector3& position, Vector3 dimensions, float inverseMass) {
	GameObject* coin = new GameObject("Coin");

	// set bounding volume
	AABBVolume* volume = new AABBVolume(dimensions);
	coin->SetBoundingVolume(volume);

	coin->GetTransform()
		.SetScale(dimensions * 2.0f)
		.SetPosition(position);

	coin->SetRenderObject(new RenderObject(coin->GetTransform(), coinMesh, glassMaterial));
	coin->GetRenderObject()->SetColour(Vector4(1.0f, 0.84f, 0.0f, 1.0f)); // golden color

	// Set physics, mass = 0, floating and staictic object
	PhysicsObject* physicsObj = new PhysicsObject(coin->GetTransform(), coin->GetBoundingVolume());
	physicsObj->SetInverseMass(0.0f);
	physicsObj->InitSphereInertia();

	coin->SetPhysicsObject(physicsObj);

	world.AddGameObject(coin);

	// add to coins list
	coins.push_back(coin);

	return coin;
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
	AABBVolume* volume = new AABBVolume(Vector3(1.0f, 1.0f, 1.0f) * meshSize);
	goose->SetBoundingVolume(volume);

	goose->GetTransform()
		.SetScale(Vector3(meshSize, meshSize, meshSize))
		.SetPosition(position);

	goose->SetRenderObject(new RenderObject(goose->GetTransform(), gooseMesh, notexMaterial));
	goose->GetRenderObject()->SetColour(Vector4(1, 0, 0, 1)); // red color goose

	PhysicsObject* physicsObj = new PhysicsObject(goose->GetTransform(), goose->GetBoundingVolume());

	physicsObj->SetInverseMass(inverseMass);
	physicsObj->InitSphereInertia();

	goose->SetPhysicsObject(physicsObj);

	world.AddGameObject(goose);

	return goose;
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

// Core Logic of Level Building
void MyGame::InitCourierLevel() {

	// Add player
	Vector3 playerStartPos = Vector3(-60, 5, 60);
	playerObject = new Player(&world, playerStartPos, catMesh, notexMaterial, Vector4(0, 1, 1, 1)); //cyan color
	world.AddGameObject(playerObject);
	
	
	// Add navGrid and AI
	if (!navGrid) {
		navGrid = new NavigationGrid("TestGrid1.txt");
	}
	rivalAI = new RivalAI(&world, navGrid, Vector3(-60, 5, 50), catMesh, notexMaterial, Vector4(1, 0, 0, 1)); // red color
	rivalAI->SetPlayer(playerObject); // 多人游戏可以设置扫描一遍所有玩家
	world.AddGameObject(rivalAI);

	// Add Goose NPC
	gooseNPC = AddGooseNPCToWorld(Vector3(-60, 5, 30));

	// Add packageObject
	packageObject = new FragileGameObject("FragilePackage", Vector3(-50, 5, 60), bonusMesh, glassMaterial, Vector4(0, 0, 1, 1)); // blue color
	world.AddGameObject(packageObject);
	playerObject->SetFragilePackage(packageObject);
	rivalAI->SetFragilePackage(packageObject);

	// Add Sphere and CubeStone
	AddSphereToWorld(Vector3(-55, 3, 60), 2.0f, 1.0f); // test sphere above package
	cubeStone = AddCubeToWorld(Vector3(-45, 3, 60), Vector3(1, 1, 1), 1.0f, "CubeStone"); // test cubeStone, interact with pressurePlate

	// Add coins at various locations, mass = 0 for floating
	AddCoinToWorld(Vector3(-50, 5, 55), Vector3(0.1f, 0.1f, 0.1f), 0.0f);
	AddCoinToWorld(Vector3(-50, 5, 10), Vector3(0.1f, 0.1f, 0.1f), 0.0f);
	AddCoinToWorld(Vector3(10, 5, 10), Vector3(0.1f, 0.1f, 0.1f), 0.0f);

	// Build terrain and obstacles
	AddFloorToWorld(Vector3(0, 0, 0)); // floor

	// walls
	float wallHeight = 20.0f;
	float boundarySize = 100.0f;
	float wallThickness = 2.0f;
	AddCubeToWorld(Vector3(0, wallHeight, -boundarySize), Vector3(boundarySize, wallHeight, wallThickness), 0.0f);
	AddCubeToWorld(Vector3(0, wallHeight, boundarySize), Vector3(boundarySize, wallHeight, wallThickness), 0.0f);
	AddCubeToWorld(Vector3(-boundarySize, wallHeight, 0), Vector3(wallThickness, wallHeight, boundarySize), 0.0f);
	AddCubeToWorld(Vector3(boundarySize, wallHeight, 0), Vector3(wallThickness, wallHeight, boundarySize), 0.0f);

	// obstacles
	AddCubeToWorld(Vector3(-30, 10, 0), Vector3(2, 10, 40), 0.0f);
	AddCubeToWorld(Vector3(40, 5, 20), Vector3(10, 5, 10), 0.0f);

	// Destination zone, green area, now it is static
	Vector3 targetPos = Vector3(60, 2, -60);
	targetZone = AddCubeToWorld(targetPos, Vector3(10, 1, 10), 0.0f);
	if (targetZone && targetZone->GetRenderObject()) {
		targetZone->GetRenderObject()->SetColour(Vector4(0, 1, 0, 0.5f)); // set green with some transparency
	}

	// Pressure plate
	pressurePlate = AddCubeToWorld(Vector3(0, 2, 40), Vector3(5, 0.5f, 5), 0.0f);
	if (pressurePlate && pressurePlate->GetRenderObject()) {
		pressurePlate->GetRenderObject()->SetColour(Vector4(1, 1, 0, 1)); // yellow
	}
	// Door controlled by pressure plate
	puzzleDoor = AddCubeToWorld(Vector3(60, 10, -30), Vector3(15, 10, 2), 0.0f);
	if (puzzleDoor && puzzleDoor->GetRenderObject()) {
		puzzleDoor->GetRenderObject()->SetColour(Vector4(0, 0, 1, 1)); // blue
	}

	// Simple Enemy AI Setup
	Vector3 enemyStartPos = Vector3(0, 5, 0);
	std::vector<Vector3> aiPath;
	float pathY = 4.0f;
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
}
