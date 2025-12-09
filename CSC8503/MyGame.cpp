#include "MyGame.h"
#include "GameWorld.h"
#include "PhysicsSystem.h"
#include "PhysicsObject.h"
#include "RenderObject.h"
#include "TextureLoader.h"
#include "NetworkObject.h"
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
	useGravity = true;
	physics.UseGravity(useGravity);

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
	localPlayerID = 0;

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

	world.GetMainCamera().UpdateCamera(dt); // Update camera

	PuzzleDoorLogic(dt); // Check Puzzle Door

	Player* localPlayer = GetLocalPlayer(); // single player / player in servant
	if (localPlayer && packageObject) {
		
		if (isTimerRunning && !isGameOver && !isGameWon) { gameDuration += dt; } // Time Logic
		if (Window::GetKeyboard()->KeyDown(KeyCodes::TAB)) { DrawHighScoreHUD(); } // press TAB to show rank
		// show time
		std::string timeStr;
		FormatTime(gameDuration, timeStr);
		Debug::Print("Time: " + timeStr, Vector2(75, 5), Vector4(1, 1, 1, 1));

		SetCameraToPlayer(localPlayer); // camera follow player

		// Show package health
		float health = packageObject->GetHealth();
		Vector4 col = (health > 50) ? Vector4(0, 1, 0, 1) : Vector4(1, 0, 0, 1); // change color based on health
		Debug::Print("Package HP: " + std::to_string((int)health), Vector2(70, 90), col);

		// for every player
		for (Player* player : players) { 
			PackageLogic(player, dt); // package logic
			GetCoinLogic(player, dt); // Coin Collection
			WinLoseLogic(player); // Win/Lose logic
			player->Update(dt);
		}

		RivalLogic();

		// Display score
		std::string scoreText = "Coins: " + std::to_string(score) + " / " + std::to_string(winningScore);
		Vector4 scoreColor = (score >= winningScore) ? Vector4(0, 1, 0, 1) : Vector4(1, 1, 0, 1); // score color change
		Debug::Print(scoreText, Vector2(75, 10), scoreColor);
		std::string rivalScoreText = "Rival Coins: " + std::to_string(rival->GetScore());
		Debug::Print(rivalScoreText, Vector2(70, 15), Debug::RED);

		// Update Keys
		UpdateKeys();

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

		// Update physics and world
		physics.Update(dt);
		world.UpdateWorld(dt);
	}
}

void MyGame::UpdateKeys() {
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
}

void MyGame::InitCamera() {
	world.GetMainCamera().SetNearPlane(0.1f);
	world.GetMainCamera().SetFarPlane(500.0f);
	world.GetMainCamera().SetPitch(-15.0f);
	world.GetMainCamera().SetYaw(315.0f);
	world.GetMainCamera().SetPosition(Vector3(-60, 40, 60));
}

void MyGame::InitWorld() {
	score = 0;
	gameDuration = 0.0f; // reset time
	isTimerRunning = true; // time start running

	world.ClearAndErase();
	physics.Clear();
	coins.clear();
	players.clear();
	score = 0;
	isGameOver = false;
	isGameWon = false;

	InitCourierLevel();
	InitDefaultPlayer();
}

void MyGame::InitDefaultPlayer() {
	Vector3 pos = Vector3(-60, 5, 60);
	Player* p = AddPlayerToWorld(pos, 1.0f);

	players.push_back(p); // add to players list
	localPlayerID = 0; // set local player ID

	// set player reference as target for rival and goose
	if (rival) rival->SetPlayerList(&players);
	if (goose) goose->SetPlayerList(&players);
	if (patrolEnemy) patrolEnemy->SetPlayerList(&players);
}

GameObject* MyGame::AddFloorToWorld(const Vector3& position) {
	GameObject* floor = new GameObject("floor");

	Vector3 floorSize = Vector3(400, 4, 400);
	AABBVolume* volume = new AABBVolume(floorSize);
	floor->SetBoundingVolume(volume);
	floor->GetTransform()
		.SetScale(floorSize)
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
	OBBVolume* volume = new OBBVolume(Vector3(dimensions.x * 5.0f, dimensions.y * 5.0f, dimensions.z));
	coin->SetBoundingVolume(volume);

	coin->GetTransform()
		.SetScale(dimensions)
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

StateGameObject* MyGame::AddPatrolEnemyToWorld(const Vector3& position, const Vector3& patrolDestination) {
	// Patroling enemy AI, if player is close, chase
	// If reaches player, transport player back to start point
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

	// set patrol route
	std::vector<Vector3> patrolPath;
	patrolPath.push_back(position);
	patrolPath.push_back(patrolDestination);
	character->SetPatrolPath(patrolPath);

	world.AddGameObject(character);

	return character;
}

Player* MyGame::AddPlayerToWorld(const NCL::Maths::Vector3& position, float radius)
{
	float inverseMass = 0.5f;

	Player* newplayer = new Player(&world);
	newplayer->SetInitPosition(position); // set initial position

	SphereVolume* volume = new SphereVolume(radius * 0.5f); // set bounding volume
	newplayer->SetBoundingVolume(volume);
	
	newplayer->GetTransform() // set transform
		.SetScale(Vector3(radius, radius, radius))
		.SetPosition(position);

	newplayer->SetRenderObject(new RenderObject(newplayer->GetTransform(), catMesh, notexMaterial));
	newplayer->GetRenderObject()->SetColour(Vector4(0, 1, 1, 1)); // cyan

	PhysicsObject* physicsObj = new PhysicsObject(newplayer->GetTransform(), newplayer->GetBoundingVolume()); // set physics object
	physicsObj->SetInverseMass(inverseMass);
	physicsObj->InitSphereInertia();
	physicsObj->SetElasticity(0.0f); // no bounciness

	newplayer->SetPhysicsObject(physicsObj);
	newplayer->SetFragilePackage(packageObject); // set package
	world.AddGameObject(newplayer);

	return newplayer;
}

RivalAI* MyGame::AddRivalAIToWorld(const NCL::Maths::Vector3& position, float radius)
{
	float inverseMass = 0.5f;

	RivalAI* newRival = new RivalAI(&world, navGrid);

	SphereVolume* volume = new SphereVolume(radius * 0.5f);
	newRival->SetBoundingVolume(volume);

	newRival->GetTransform()
		.SetScale(Vector3(radius, radius, radius))
		.SetPosition(position);

	newRival->SetRenderObject(new RenderObject(newRival->GetTransform(), catMesh, notexMaterial));
	newRival->GetRenderObject()->SetColour(Vector4(1, 0, 0, 1)); // red

	PhysicsObject* physicsObj = new PhysicsObject(newRival->GetTransform(), newRival->GetBoundingVolume());
	physicsObj->SetInverseMass(inverseMass);
	physicsObj->InitSphereInertia();
	physicsObj->SetElasticity(0.0f); // no bounciness

	newRival->SetPhysicsObject(physicsObj);

	newRival->SetWinningScore(winningScore); // set rival winning condition

	world.AddGameObject(newRival);

	return newRival;
}

GooseNPC* MyGame::AddGooseNPCToWorld(const Vector3& position, float radius)
{
	float inverseMass = 0.5f;

	GooseNPC* newGoose = new GooseNPC(navGrid); // init goose with navgrid and player reference
	SphereVolume* volume = new SphereVolume(radius);
	newGoose->SetBoundingVolume(volume);

	newGoose->GetTransform()
		.SetScale(Vector3(radius, radius, radius))
		.SetPosition(position);

	newGoose->SetRenderObject(new RenderObject(newGoose->GetTransform(), gooseMesh, notexMaterial));
	newGoose->GetRenderObject()->SetColour(Vector4(1, 0, 0, 1)); // red color goose

	PhysicsObject* physicsObj = new PhysicsObject(newGoose->GetTransform(), newGoose->GetBoundingVolume());

	physicsObj->SetInverseMass(inverseMass);
	physicsObj->InitSphereInertia();

	newGoose->SetPhysicsObject(physicsObj);

	world.AddGameObject(newGoose);

	return newGoose;
}

// Game logic functions

void MyGame::SetCameraToPlayer(Player* player) {
	Vector3 playerPos = player->GetTransform().GetPosition();

	float yaw = world.GetMainCamera().GetYaw();
	float pitch = world.GetMainCamera().GetPitch();

	// ristrict pitch angle
	if (pitch > 30.0f) pitch = 30.0f;
	if (pitch < -60.0f) pitch = -60.0f;

	// calculate camera direction
	Quaternion cameraRot = Quaternion::EulerAnglesToQuaternion(pitch, yaw, 0);
	Vector3 cameraBackward = cameraRot * Vector3(0, 0, 1);

	// set desired distance and offset
	float dist = 15.0f;
	Vector3 offset = Vector3(0, 5, 0); // above player's head

	// Avoid Camera Collision
	float maxDist = 15.0f;
	float currentDist = maxDist;
	// fire ray from player to camera
	Vector3 rayOrigin = playerPos + offset;
	Vector3 rayDir = cameraBackward;
	Ray ray(rayOrigin, rayDir);
	RayCollision collision;
	// if ray hits an object
	if (world.Raycast(ray, collision, true, player)) {
		if (collision.rayDistance < maxDist) {
			// make sure camera is not in the wall
			currentDist = collision.rayDistance - 0.5f;
			if (currentDist < 0.5f) currentDist = 0.5f;
		}
	}

	Vector3 cameraPos = rayOrigin + (cameraBackward * currentDist);
	world.GetMainCamera().SetPosition(cameraPos);
}

void MyGame::PuzzleDoorLogic(float dt) {
	if (!puzzleDoor || !pressurePlate) return;
	bool isTriggered = false;
	Vector3 platePos = pressurePlate->GetTransform().GetPosition();
	Vector3 triggerSize = Vector3(5.5f, 2.0f, 5.5f);

	auto CheckTrigger = [&](GameObject* obj) {
		if (!obj) return false;
		Vector3 pos = obj->GetTransform().GetPosition();
		// check AABB
		return (pos.x > platePos.x - triggerSize.x && pos.x < platePos.x + triggerSize.x &&
			pos.y > platePos.y - triggerSize.y && pos.y < platePos.y + triggerSize.y &&
			pos.z > platePos.z - triggerSize.z && pos.z < platePos.z + triggerSize.z);
		};

	// if cubeStone is on the pressure plate
	if (CheckTrigger(cubeStone)) {
		isTriggered = true;
	}

	// door movement
	Vector3 doorPos = puzzleDoor->GetTransform().GetPosition();
	float targetY = isTriggered ? -10.0f : 10.0f;
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
		pressurePlate->GetRenderObject()->SetColour(Vector4(0, 1, 0, 1)); // turn green
	}
	else {
		pressurePlate->GetRenderObject()->SetColour(Vector4(1, 1, 0, 1)); // default yellow
	}
}

void MyGame::GetCoinLogic(Player* player, float dt) {
	for (int i = coins.size() - 1; i >= 0; --i) {
		GameObject* c = coins[i];

		// make coin rotate
		float rotationSpeed = 90.0f;
		Quaternion rotation = Quaternion::AxisAngleToQuaterion(Vector3(0, 1, 0), rotationSpeed * dt);
		Quaternion currentOri = c->GetTransform().GetOrientation();
		c->GetTransform().SetOrientation(rotation * currentOri);

		if (!c->IsActive()) continue;
		// calculate distance to player
		float player_dist = Vector::Length((player->GetTransform().GetPosition() - c->GetTransform().GetPosition()));
		GameObject* playerHeld = player->GetHeldItem();
		// collection radius 2.5f, need FragilePackage to collect
		if (playerHeld) {
			if (player_dist < 2.5f && playerHeld->GetName() == "FragilePackage") {
				c->GetRenderObject()->SetColour(Vector4(0, 0, 0, 0)); // Set it transparent, which means it is removed
				packageObject->IncreaseCollectionCount(); // increase package collection count
				coins.erase(coins.begin() + i); // remove coin from vector
			}
		}
	}
}

void MyGame::PackageLogic(Player* player, float dt) {
	if (!player || !packageObject) return;

	if (player->GetHeldItem() && player->GetHeldItem()->GetName() == "FragilePackage") {
		score = packageObject->GetCollectionCount(); // reset player score
		if(packageObject->IsBroken()) {
			Debug::Print("PACKAGE BROKEN!", Vector2(30, 50), Vector4(1, 0, 0, 1));
			player->ThrowHeldItem(Vector3(0, 0, 0)); // drop broken package
			score = 0; // reset player score
		}
	}
	packageObject->Update(dt);
}

void MyGame::WinLoseLogic(Player* player){
	// Win Condition
	if (!isGameWon && winZone) {
		Vector3 zonePos = winZone->GetTransform().GetPosition();
		Vector3 zoneSize = Vector3(10, 1, 10); // size of the target zone

		Vector3 PackagePos = packageObject->GetTransform().GetPosition();

		bool inZone = (PackagePos.x > zonePos.x - zoneSize.x && PackagePos.x < zonePos.x + zoneSize.x &&
			PackagePos.z > zonePos.z - zoneSize.z && PackagePos.z < zonePos.z + zoneSize.z);

		// score && inZone -> win
		if (inZone && score >= winningScore) {
			isGameWon = true;
			isTimerRunning = false;
		}
		// score is not enough
		if (inZone && score < winningScore) {
			Debug::Print("Need more coins!", Vector2(40, 40), Vector4(1, 0, 0, 1));
		}
		// Lose
		if (inZone && rival->GetScore() >= winningScore) {
			isGameOver = true;
			isTimerRunning = false;
			gameOverReason = GameOverReason::RivalWin;
		}
	}
	// Death Condition - hit by goose
	if (!isGameOver && !isGameWon && player && goose) {
		Vector3 pPos = player->GetTransform().GetPosition();
		Vector3 gPos = goose->GetTransform().GetPosition();

		// simple distance check, large than goose size
		float dist = Vector::Length((pPos - gPos));
		if (dist < 4.1f) {
			isGameOver = true;
			gameOverReason = GameOverReason::GooseCatch;
		}
	}
}

void MyGame::RivalLogic(){
	// rival has the same logic as player
	if (!rival || !packageObject) return;
	// About coin collection
	for (int i = coins.size() - 1; i >= 0; --i) {
		GameObject* c = coins[i];

		if (!c->IsActive()) continue;
		float rival_dist = Vector::Length((rival->GetTransform().GetPosition() - c->GetTransform().GetPosition()));
		GameObject* rivalHeld = rival->GetHeldItem();
		
		if (rivalHeld) {
			if (rival_dist < 2.5f && rivalHeld->GetName() == "FragilePackage") {
				world.RemoveGameObject(c, true); // remove coin from world
				packageObject->IncreaseCollectionCount(); // increase package collection count
				coins.erase(coins.begin() + i); // remove coin from vector
			}
		}
	}

	// Update rival score
	if (rival->GetHeldItem() != nullptr && rival->GetHeldItem()->GetName() == "FragilePackage") {
		rival->SetScore(packageObject->GetCollectionCount());
		if (packageObject->IsBroken()) {
			rival->ThrowHeldItem(Vector3(0, 0, 0));
			rival->SetScore(0);
		}
	}
}

// Auxiliary Functions

void MyGame::FormatTime(float time, std::string& outStr) {
	int minutes = (int)time / 60;
	int seconds = (int)time % 60;
	std::stringstream ss;
	ss << std::setfill('0') << std::setw(2) << minutes << ":"
		<< std::setfill('0') << std::setw(2) << seconds;
	outStr = ss.str();
}

void MyGame::DrawHighScoreHUD() {
	// 【关键修改】根据当前模式获取对应的榜单
	auto& scores = HighScoreManager::Instance().GetScores(isNetworkGame);

	float startY = 30.0f;
	std::string title = isNetworkGame ? "--- NETWORK BEST ---" : "--- SINGLE BEST ---";
	Vector4 color = isNetworkGame ? Vector4(0, 1, 1, 1) : Vector4(1, 0.8f, 0, 1); // 网络蓝，单机黄

	Debug::Print(title, Vector2(40, startY), color);

	for (size_t i = 0; i < scores.size(); ++i) {
		std::string timeStr;
		FormatTime(scores[i].time, timeStr);
		std::string entry = std::to_string(i + 1) + ". " + std::string(scores[i].name) + " - " + timeStr;
		Debug::Print(entry, Vector2(35, startY + 5 + (i * 5)), Vector4(1, 1, 1, 1));
	}
}

// Core Logic of Level Building
void MyGame::InitCourierLevel() {
	// Add navGrid and AI
	if (!navGrid) {
		navGrid = new NavigationGrid("TestGrid1.txt");
	}
	rival = AddRivalAIToWorld(Vector3(-55, 5, 0), 1.0f); // red color
	goose = AddGooseNPCToWorld(Vector3(-60, 5, 0), 3.0f);

	// Add Enemy that patrols and targets the player
	patrolEnemy = AddPatrolEnemyToWorld(Vector3(20, 4, 20), Vector3(20, 4, -20));
	patrolEnemy->SetGameWorld(&world);

	// Add packageObject
	packageObject = new FragileGameObject("FragilePackage", Vector3(-50, 5, 60), bonusMesh, glassMaterial, Vector4(0, 0, 1, 1)); // blue color
	
	world.AddGameObject(packageObject);
	rival->SetFragilePackage(packageObject);

	// Add Sphere and CubeStone
	AddSphereToWorld(Vector3(-55, 5, 60), 2.0f, 1.0f); // test sphere above package
	cubeStone = AddCubeToWorld(Vector3(-45, 5, 60), Vector3(1, 1, 1), 0.5f, "CubeStone"); // test cubeStone, interact with pressurePlate

	// Add coins at various locations, mass = 0 for floating
	AddCoinToWorld(Vector3(-50, 5, 55), Vector3(0.1f, 0.1f, 0.1f), 0.0f);
	AddCoinToWorld(Vector3(-50, 5, 10), Vector3(0.1f, 0.1f, 0.1f), 0.0f);
	AddCoinToWorld(Vector3(10, 5, 10), Vector3(0.1f, 0.1f, 0.1f), 0.0f);

	// Build terrain and obstacles
	AddFloorToWorld(Vector3(0, -1, 0)); // floor

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
	Vector3 targetPos = Vector3(60, 1, -60);
	winZone = AddCubeToWorld(targetPos, Vector3(10, 0.5f, 10), 0.0f, "winZone");
	if (winZone && winZone->GetRenderObject()) {
		winZone->GetRenderObject()->SetColour(Vector4(0, 1, 0, 0.5f)); // set green with some transparency
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
}
