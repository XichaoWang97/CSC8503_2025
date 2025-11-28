#pragma once
#include "PushdownState.h"
#include "MyGame.h"
#include "Window.h"
#include "Debug.h"

#include "PhysicsSystem.h"
#include "GameWorld.h"

#include "NetworkedGame.h"
#include "GameServer.h"
#include "GameClient.h"

namespace NCL {
	namespace CSC8503 {

		class MainMenuState;
		class SinglePlayerState;
		class PauseState;
		class NetworkedGameState;

		// --- 1. 暂停状态 ---
		class PauseState : public PushdownState {
		public:
			PauseState(MyGame* g) : game(g) {}
			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				Debug::Print("Paused", Vector2(45, 40), Debug::WHITE);
				Debug::Print("Press U to Unpause", Vector2(35, 50), Debug::WHITE);
				Debug::Print("Press M to Return to Menu", Vector2(30, 55), Debug::WHITE);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::U)) {
					return PushdownResult::Pop;
				}
				if (Window::GetKeyboard()->KeyPressed(KeyCodes::M)) {
					return PushdownResult::Pop;
				}
				return PushdownResult::NoChange;
			}
		protected:
			MyGame* game;
		};

		// --- 2. 单人游戏状态 ---
		class SinglePlayerState : public PushdownState {
		public:
			// 构造函数现在接收核心系统
			SinglePlayerState(MyGame* g, GameWorld* w, PhysicsSystem* p, bool reset = true)
				: game(g), world(w), physics(p) {
				if (reset) {
					game->ResetGame();
				}
			}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				game->UpdateGame(dt);

				// --- 修复: 直接调用系统 ---
				physics->Update(dt);
				world->UpdateWorld(dt);

				Debug::Print("Press ESC to Pause", Vector2(5, 5), Debug::WHITE);
				Debug::Print("Press F1 to Main Menu", Vector2(5, 10), Debug::WHITE);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::ESCAPE)) {
					*newState = new PauseState(game);
					return PushdownResult::Push;
				}
				if (Window::GetKeyboard()->KeyPressed(KeyCodes::F1)) {
					return PushdownResult::Pop;
				}
				return PushdownResult::NoChange;
			}

			void OnAwake() override {
				Window::GetWindow()->ShowOSPointer(false);
				Window::GetWindow()->LockMouseToWindow(true);
			}

		protected:
			MyGame* game;
			GameWorld* world;
			PhysicsSystem* physics;
		};

		// --- 任务 2.2: 网络游戏状态 ---
		class NetworkedGameState : public PushdownState {
		public:
			// 构造函数现在接收核心系统
			NetworkedGameState(NetworkedGame* g, GameWorld* w, PhysicsSystem* p)
				: netGame(g), world(w), physics(p) {
			}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				// netGame->UpdateGame(dt); // 网络游戏逻辑由其内部驱动

				// --- 修复: 直接调用系统 ---
				physics->Update(dt);
				world->UpdateWorld(dt);

				Debug::Print("Press F1 to Disconnect", Vector2(5, 10), Debug::WHITE);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::F1)) {
					return PushdownResult::Pop; // 返回主菜单
				}
				return PushdownResult::NoChange;
			}

			void OnAwake() override {
				Window::GetWindow()->ShowOSPointer(false);
				Window::GetWindow()->LockMouseToWindow(true);
			}
		protected:
			NetworkedGame* netGame;
			GameWorld* world; // 新增
			PhysicsSystem* physics; // 新增
		};

		// --- 任务 2.2: 客户端状态 ---
		class ClientGameState : public PushdownState {
		public:
			// 构造函数需要传递系统指针
			ClientGameState(NetworkedGame* g, GameWorld* w, PhysicsSystem* p)
				: netGame(g), world(w), physics(p) {
				client = new GameClient();
				// netGame->SetClient(client); 
			}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				Debug::Print("Connecting to server...", Vector2(30, 40), Debug::WHITE);
				Debug::Print("127.0.0.1 (Hardcoded)", Vector2(30, 45), Debug::WHITE);

				if (client->Connect(127, 0, 0, 1, 1234)) {
					// 连接成功，进入网络游戏状态 (并传递指针)
					*newState = new NetworkedGameState(netGame, world, physics);
					return PushdownResult::Push;
				}

				Debug::Print("Failed to connect!", Vector2(30, 50), Debug::RED);
				Debug::Print("Press F1 to return", Vector2(30, 55), Debug::WHITE);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::F1)) {
					return PushdownResult::Pop;
				}
				return PushdownResult::NoChange;
			}
		protected:
			NetworkedGame* netGame;
			GameWorld* world; // 新增
			PhysicsSystem* physics; // 新增
			GameClient* client;
		};

		// --- 任务 2.2: 服务器状态 ---
		class HostGameState : public PushdownState {
		public:
			// 构造函数需要传递系统指针
			HostGameState(NetworkedGame* g, GameWorld* w, PhysicsSystem* p)
				: netGame(g), world(w), physics(p) {
				server = new GameServer(1234, 4);
				// netGame->SetServer(server);
			}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				Debug::Print("Hosting on port 1234...", Vector2(30, 40), Debug::WHITE);
				Debug::Print("Waiting for players...", Vector2(30, 45), Debug::WHITE);
				Debug::Print("Press 1 to Start Game", Vector2(30, 55), Debug::WHITE);
				Debug::Print("Press F1 to Cancel", Vector2(30, 60), Debug::WHITE);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::NUM1)) {
					// 开始游戏 (并传递指针)
					*newState = new NetworkedGameState(netGame, world, physics);
					return PushdownResult::Push;
				}
				if (Window::GetKeyboard()->KeyPressed(KeyCodes::F1)) {
					return PushdownResult::Pop;
				}
				return PushdownResult::NoChange;
			}
		protected:
			NetworkedGame* netGame;
			GameWorld* world; // 新增
			PhysicsSystem* physics; // 新增
			GameServer* server;
		};

		// --- 3. 主菜单状态 ---
		class MainMenuState : public PushdownState {
		public:
			// --- 任务 2.2 修改: 构造函数需要 world 和 physics ---
			MainMenuState(MyGame* g, NetworkedGame* ng, GameWorld* w, PhysicsSystem* p)
				: game(g), netGame(ng), world(w), physics(p) {
			}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				Debug::Print("CSC8503 Coursework", Vector2(35, 20), Debug::CYAN);
				Debug::Print("1. Start Single Player (Part A)", Vector2(30, 40), Debug::WHITE);
				Debug::Print("2. Host Network Game (Server)", Vector2(30, 45), Debug::WHITE);
				Debug::Print("3. Join Network Game (Client)", Vector2(30, 50), Debug::WHITE);
				Debug::Print("Esc. Exit Game", Vector2(30, 60), Debug::WHITE);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::NUM1)) {
					*newState = new SinglePlayerState(game, world, physics, true);
					return PushdownResult::Push;
				}

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::NUM2)) {
					*newState = new HostGameState(netGame, world, physics);
					return PushdownResult::Push;
				}
				if (Window::GetKeyboard()->KeyPressed(KeyCodes::NUM3)) {
					*newState = new ClientGameState(netGame, world, physics);
					return PushdownResult::Push;
				}

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::ESCAPE)) {
					return PushdownResult::Pop;
				}

				return PushdownResult::NoChange;
			}

			void OnAwake() override {
				Window::GetWindow()->ShowOSPointer(true);
				Window::GetWindow()->LockMouseToWindow(false);
			}

		protected:
			MyGame* game;
			NetworkedGame* netGame;
			GameWorld* world;
			PhysicsSystem* physics;
		};
	}
}