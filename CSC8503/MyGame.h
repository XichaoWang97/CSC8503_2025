#pragma once
#pragma once
#include "RenderObject.h"
#include "StateGameObject.h"
#include "FragileGameObject.h"
#include "NavigationGrid.h"
#include "GooseNPC.h"
#include "RivalAI.h"
#include "Player.h"
#include "HighScoreManager.h"

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

		enum class GameOverReason {
			None,
			GooseCatch, // catched by goose
			RivalWin,    // rival win
			PlayerWin
		};

		class MyGame {
		public:
			MyGame(GameWorld& gameWorld, GameTechRendererInterface& renderer, PhysicsSystem& physics);
			~MyGame();

			virtual void UpdateGame(float dt);

			// Reset the game to the initial state
			virtual void ResetGame() {
				InitWorld();
			}

			GameWorld* GetGameWorld() const { return &world; }
			PhysicsSystem* GetPhysics() const { return &physics; }

			bool IsGameOver() const { return isGameOver; }
			bool IsGameWon()  const { return isGameWon; }
			
			Player* GetLocalPlayer() const { 
				if (players.empty()) return nullptr; 
				if (localPlayerID >= 0 && localPlayerID < players.size()) {
					return players[localPlayerID];
				}
				return nullptr;
			}

			GameOverReason GetGameOverReason() const { return gameOverReason; }
			float GetGameDuration() const { return gameDuration; }
			void DrawHighScoreHUD();   // draw high score rank
			void FormatTime(float time, std::string& outStr); // Time: MM:SS

		protected:
			// Time
			float gameDuration = 0.0f;
			bool  isTimerRunning = false;

			bool isNetworkGame = false;

			void InitCamera();
			void InitWorld();
			void InitCourierLevel();
			virtual void InitDefaultPlayer();
			virtual void UpdateKeys();

			// Add specific game objects to the world
			GameObject* AddFloorToWorld(const NCL::Maths::Vector3& position);
			GameObject* AddSphereToWorld(const NCL::Maths::Vector3& position, float radius, float inverseMass = 10.0f); // also as stones
			GameObject* AddCubeToWorld(const NCL::Maths::Vector3& position, NCL::Maths::Vector3 dimensions, float inverseMass = 10.0f, std::string name = "Terrain");
			GameObject* AddCoinToWorld(const NCL::Maths::Vector3& position, Vector3 dimensions, float inverseMass = 1.0f);
			StateGameObject* AddPatrolEnemyToWorld(const NCL::Maths::Vector3& position, const Vector3& patrolDestination);
			Player* AddPlayerToWorld(const NCL::Maths::Vector3& position, float radius);
			RivalAI* AddRivalAIToWorld(const NCL::Maths::Vector3& position, float radius);
			GooseNPC* AddGooseNPCToWorld(const NCL::Maths::Vector3& position, float radius);

			// Game logic functions
			void SetCameraToPlayer(Player* player);
			void PuzzleDoorLogic(float dt);
			virtual void GetCoinLogic(Player* player, float dt);
			virtual void PackageLogic(Player* player, float dt);
			virtual void WinLoseLogic(Player* player);
			virtual void RivalLogic();
			void InitHedgeMaze();

			// pointers to specific game objects
			GameObject* winZone = nullptr;  // destination
			GameObject* puzzleDoor = nullptr;
			GameObject* pressurePlate = nullptr;
			GameObject* sphereStone = nullptr;
			GameObject* cubeStone = nullptr;  // cube to interact with pressure plate
			GameObject* coinBonus = nullptr;  // bonus coin
			GooseNPC* goose = nullptr;
			RivalAI* rival = nullptr;
			FragileGameObject* packageObject = nullptr;
			StateGameObject* patrolEnemy = nullptr;
			
			// players stored in a list
			std::vector<Player*> players;
			int localPlayerID = 0;
			
			// About coin and score
			int score = 0;
			int scoreInPackage = 0; // coins collected while carrying package
			const int winningScore = 3; // score needed to win
			std::vector<GameObject*> coins; // coin list

			// win or lose
			bool isGameOver = false;
			bool isGameWon = false;
			GameOverReason gameOverReason = GameOverReason::None;

			// for navigation and pathfinding
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
		};
	}
}