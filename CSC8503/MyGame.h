#pragma once
#pragma once
#include "RenderObject.h"
#include "StateGameObject.h"
#include "FragileGameObject.h"
#include "NavigationGrid.h"
#include "GooseNPC.h"
#include "RivalAI.h"
#include "Player.h"

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
		class PositionConstraint;

		class MyGame {
		public:
			MyGame(GameWorld& gameWorld, GameTechRendererInterface& renderer, PhysicsSystem& physics);
			~MyGame();

			virtual void UpdateGame(float dt);

			// Reset the game to the initial state
			void ResetGame() {
				InitWorld();
			}

			GameWorld* GetGameWorld() const { return &world; }
			PhysicsSystem* GetPhysics() const { return &physics; }

			bool IsGameOver() const { return isGameOver; }
			bool IsGameWon()  const { return isGameWon; }

		protected:
			void InitCamera();
			void InitWorld();

			void InitCourierLevel();

			// Add specific game objects to the world
			GameObject* AddFloorToWorld(const NCL::Maths::Vector3& position);
			GameObject* AddSphereToWorld(const NCL::Maths::Vector3& position, float radius, float inverseMass = 10.0f); // also as stones
			GameObject* AddCubeToWorld(const NCL::Maths::Vector3& position, NCL::Maths::Vector3 dimensions, float inverseMass = 10.0f, std::string name = "Terrain");
			GameObject* AddCoinToWorld(const NCL::Maths::Vector3& position, Vector3 dimensions, float inverseMass = 1.0f);
			StateGameObject* AddEnemyToWorld(const NCL::Maths::Vector3& position);
			Player* AddPlayerToWorld(const NCL::Maths::Vector3& position, float radius);
			RivalAI* AddRivalAIToWorld(const NCL::Maths::Vector3& position, float radius);
			GooseNPC* AddGooseNPCToWorld(const NCL::Maths::Vector3& position, float radius);

			// pointers to specific game objects
			GameObject* targetZone = nullptr;  // destination
			GameObject* puzzleDoor = nullptr;
			GameObject* pressurePlate = nullptr;
			GameObject* cubeStone = nullptr;  // cube to interact with pressure plate
			GameObject* coinBonus = nullptr;  // bonus coin
			Player* player = nullptr;
			RivalAI* rival = nullptr;
			GooseNPC* goose = nullptr;
			FragileGameObject* packageObject = nullptr;
			
			// About coin and score
			int score = 0;
			const int winningScore = 3; // score needed to win
			std::vector<GameObject*> coins; // coin list
			// win or lose
			bool isGameOver = false;
			bool isGameWon = false;
			// 用于存储导航网格
			NavigationGrid* navGrid = nullptr;
			

			GameWorld& world;
			GameTechRendererInterface& renderer;
			PhysicsSystem& physics;
			Controller* controller;

			bool useGravity;
			bool inSelectionMode;

			float		forceMagnitude;

			GameObject* selectionObject = nullptr;
			
			// Meshes Textures and Materials
			Rendering::Mesh* catMesh = nullptr;
			Rendering::Mesh* kittenMesh = nullptr;
			Rendering::Mesh* enemyMesh = nullptr;
			Rendering::Mesh* bonusMesh = nullptr;
			Rendering::Mesh* gooseMesh = nullptr;
			Rendering::Mesh* capsuleMesh = nullptr;
			Rendering::Mesh* cubeMesh = nullptr;
			Rendering::Mesh* sphereMesh = nullptr;
			Rendering::Mesh* coinMesh = nullptr;
			Rendering::Texture* defaultTex = nullptr;
			Rendering::Texture* checkerTex = nullptr;
			Rendering::Texture* glassTex = nullptr;
			GameTechMaterial checkerMaterial;
			GameTechMaterial glassMaterial;
			GameTechMaterial notexMaterial;

			GameObject* objClosest = nullptr;

			void BridgeConstraintTest();

			

			// --- 抓取系统核心结构 ---
			struct GrappleInfo {
				GameObject* holder;       // 谁拿着
				GameObject* item;         // 拿着什么
				PositionConstraint* constraint; // 那个物理约束
			};
			std::vector<GrappleInfo> activeGrapples; // 当前所有的抓取关系
		};
	}
}