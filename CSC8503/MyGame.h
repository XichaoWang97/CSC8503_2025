#pragma once
#pragma once
#include "RenderObject.h"
#include "StateGameObject.h"
#include "FragileGameObject.h"
#include "NavigationGrid.h"
#include "GooseNPC.h"
#include "RivalAI.h"

namespace NCL {
	class Controller;

	namespace Rendering {
		class Mesh;
		class Texture;
		class Shader;
	}
	namespace CSC8503 {
		class GameTechRendererInterface;
		class PhysicsSystem;
		class GameWorld;
		class GameObject;
		class PositionConstraint; // For the pressure plate/door puzzle

		class MyGame {
		public:
			MyGame(GameWorld& gameWorld, GameTechRendererInterface& renderer, PhysicsSystem& physics);
			~MyGame();

			virtual void UpdateGame(float dt);

			// --- 任务 2.1 新增: 公开重置接口 ---
			void ResetGame() {
				InitWorld();
			}
			// --- 任务 2.1 新增: 获取核心系统引用 (供状态机 Update 使用) ---
			GameWorld* GetGameWorld() const { return &world; }
			PhysicsSystem* GetPhysics() const { return &physics; }

		protected:
			void InitCamera();
			void InitWorld();

			void InitCourierLevel();

			// Add specific game objects to the world
			GameObject* AddPlayerToWorld(const NCL::Maths::Vector3& position);
			GameObject* AddFloorToWorld(const NCL::Maths::Vector3& position);
			GameObject* AddSphereToWorld(const NCL::Maths::Vector3& position, float radius, float inverseMass = 10.0f);
			GameObject* AddCubeToWorld(const NCL::Maths::Vector3& position, NCL::Maths::Vector3 dimensions, float inverseMass = 10.0f);
			StateGameObject* AddEnemyToWorld(const NCL::Maths::Vector3& position);

			RivalAI* AddRivalAIToWorld(const NCL::Maths::Vector3& position);
			GooseNPC* AddGooseNPCToWorld(const NCL::Maths::Vector3& position);

			// --- 任务 0.2 新增: 关卡特定对象指针 ---
			GameObject* targetZone = nullptr;   // 终点区域
			GameObject* puzzleDoor = nullptr;   // 谜题门
			GameObject* pressurePlate = nullptr; // 压力板
			// --- 任务 0.3 新增: 玩家指针与控制 ---
			GameObject* playerObject = nullptr;
			virtual void PlayerControl(float dt);
			// --- 修复跳跃: 射线检测地面 ---
			bool IsPlayerOnGround(GameObject* obj);
			// --- 任务 1.4 新增: 创建易碎包裹 ---
			FragileGameObject* AddFragilePackageToWorld(const NCL::Maths::Vector3& position);
			FragileGameObject* packageObject = nullptr;
			
			GameWorld& world;
			GameTechRendererInterface& renderer;
			PhysicsSystem& physics;
			Controller* controller;

			bool useGravity;
			bool inSelectionMode;

			float		forceMagnitude;

			GameObject* selectionObject = nullptr;

			Rendering::Mesh* capsuleMesh = nullptr;
			Rendering::Mesh* cubeMesh = nullptr;
			Rendering::Mesh* sphereMesh = nullptr;

			Rendering::Texture* defaultTex = nullptr;
			Rendering::Texture* checkerTex = nullptr;
			Rendering::Texture* glassTex = nullptr;

			//Coursework Meshes
			Rendering::Mesh* catMesh = nullptr;
			Rendering::Mesh* kittenMesh = nullptr;
			Rendering::Mesh* enemyMesh = nullptr;
			Rendering::Mesh* bonusMesh = nullptr;
			Rendering::Mesh* gooseMesh = nullptr;

			GameTechMaterial checkerMaterial;
			GameTechMaterial glassMaterial;
			GameTechMaterial notexMaterial;

			//Coursework Additional functionality	
			GameObject* lockedObject = nullptr;
			NCL::Maths::Vector3 lockedOffset = NCL::Maths::Vector3(0, 14, 20);
			void LockCameraToObject(GameObject* o) {
				lockedObject = o;
			}

			GameObject* objClosest = nullptr;

			void BridgeConstraintTest();

			// [新增] 用于存储导航网格
			NavigationGrid* navGrid = nullptr;
			GooseNPC* gooseNPC = nullptr;
			RivalAI* rivalAI = nullptr;

			// ---------------------
			bool SelectObject();
			void MoveSelectedObject();
			void DebugObjectMovement();
			void LockedObjectMovement();
			// ---------------------
		};
	}
}