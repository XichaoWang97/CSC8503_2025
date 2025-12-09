#pragma once
#include "PushdownState.h"
#include "MyGame.h"
#include "Window.h"
#include "Debug.h"

#include "PhysicsSystem.h"
#include "GameWorld.h"

#include "NetworkedGame.h"

namespace NCL {
	namespace CSC8503 {

		class MainMenuState;
		class SinglePlayerState;
		class PauseState;
		class NetworkedGameState;

		// Pause state
		class PauseState : public PushdownState {
		public:
			PauseState(MyGame* g) : game(g) {}
			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				Debug::Print("Paused", Vector2(45, 40), Debug::WHITE);
				Debug::Print("Press ESC to Unpause", Vector2(35, 50), Debug::WHITE);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::ESCAPE)) {
					return PushdownResult::Pop;
				}

				return PushdownResult::NoChange;
			}
		protected:
			MyGame* game;
		};

		// DeadState
		class DeadState : public PushdownState {
		public:
			DeadState(MyGame* g, GameOverReason reason) : game(g), reason(reason) {}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				Debug::Print("GameOver", Vector2(40, 40), Debug::RED);
				// different reasons print different words
				if (reason == GameOverReason::GooseCatch) {
					Debug::Print("Player Caught by Goose!", Vector2(25, 50), Debug::RED);
				}
				else if (reason == GameOverReason::RivalWin) {
					Debug::Print("The Rival Delivered the Package First!", Vector2(20, 50), Debug::RED);
					Debug::Print("You were too slow!", Vector2(35, 55), Debug::RED);
				}
				Debug::Print("Press F1 to Restart", Vector2(30, 60), Debug::RED);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::F1)) {
					game->ResetGame();
					return PushdownResult::Pop;
				}
				return PushdownResult::NoChange;
			}

		protected:
			MyGame* game;
			GameOverReason reason;
		};

		// WinState
		class WinState : public PushdownState {
		public:
			WinState(MyGame* g) : game(g) {}
			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				Debug::Print("MISSION SUCCESS!", Vector2(35, 40), Debug::GREEN);
				Debug::Print("Coins Collected!", Vector2(35, 50), Debug::GREEN);
				Debug::Print("Press F1 to Restart", Vector2(30, 60), Debug::GREEN);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::F1)) {
					game->ResetGame();
					return PushdownResult::Pop;
				}
				return PushdownResult::NoChange;
			}
		protected:
			MyGame* game;
		};

		// Single player state
		class SinglePlayerState : public PushdownState {
		public:
			SinglePlayerState(MyGame* g, GameWorld* w, PhysicsSystem* p, bool reset = true)
				: game(g), world(w), physics(p) {
				if (reset) {
					game->ResetGame();
				}
			}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				game->UpdateGame(dt);

				// check game over conditions
				if (game->IsGameOver()) {
					*newState = new DeadState(game, game->GetGameOverReason());
					return PushdownResult::Push;
				}

				// check game win conditions
				if (game->IsGameWon()) {
					*newState = new WinState(game);
					return PushdownResult::Push;
				}

				Debug::Print("Press ESC to Pause", Vector2(5, 5), Debug::YELLOW);
				Debug::Print("Press 1 to Return to Main Menu", Vector2(5, 8), Debug::YELLOW);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::ESCAPE)) {
					*newState = new PauseState(game);
					return PushdownResult::Push;
				}
				if (Window::GetKeyboard()->KeyPressed(KeyCodes::NUM1)) {
					return PushdownResult::Pop;
				}
				// Debug Drawing volume
				if (Window::GetKeyboard()->KeyDown(KeyCodes::NUM2)) {
					physics->DrawDebugData();
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

		// Networked game state
		class NetworkedGameState : public PushdownState {
		public:
			NetworkedGameState(NetworkedGame* g)
				: netGame(g) {
			}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				// 【关键修改】必须调用 netGame 的 UpdateGame，因为网络收发包逻辑都在这里面
				// 且 NetworkedGame::UpdateGame 内部会调用 physics->Update，所以不要在这里重复调用
				netGame->UpdateGame(dt);

				Debug::Print("Multiplayer Mode", Vector2(5, 5), Debug::GREEN);
				Debug::Print("Press ESC to Disconnect", Vector2(5, 10), Debug::WHITE);
				// check game over conditions
				if (netGame->IsGameOver()) {
					*newState = new DeadState(netGame, netGame->GetGameOverReason());
					return PushdownResult::Push;
				}

				// check game win conditions
				if (netGame->IsGameWon()) {
					*newState = new WinState(netGame);
					return PushdownResult::Push;
				}

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::ESCAPE)) {
					netGame->Disconnect();
					return PushdownResult::Pop;
				}
				return PushdownResult::NoChange;
			}

			void OnAwake() override {
				Window::GetWindow()->ShowOSPointer(false);
				Window::GetWindow()->LockMouseToWindow(true);
			}

		protected:
			NetworkedGame* netGame;
		};

		// Client game state
		class ClientGameState : public PushdownState {
		public:
			ClientGameState(NetworkedGame* g)
				: netGame(g) {
				// 不在这里创建 GameClient，而是让 NetworkedGame 去初始化
				// 暂时注释掉这里的自动连接，改为在 Update 里按键触发，或者直接在这里调用
			}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				Debug::Print("Waiting to Connect...", Vector2(30, 40), Debug::WHITE);
				Debug::Print("Press SPACE to Connect Localhost", Vector2(30, 45), Debug::WHITE);
				Debug::Print("Press ESC to Return", Vector2(30, 50), Debug::WHITE);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::SPACE)) {
					// 【关键修改】调用 NetworkedGame 的方法来启动客户端
					netGame->StartAsClient(127, 0, 0, 1);

					// 进入游戏循环状态
					*newState = new NetworkedGameState(netGame);
					return PushdownResult::Push;
				}

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::ESCAPE)) {
					return PushdownResult::Pop;
				}
				return PushdownResult::NoChange;
			}
		protected:
			NetworkedGame* netGame;
		};

		// Host game state(server)
		class HostGameState : public PushdownState {
		public:
			HostGameState(NetworkedGame* g)
				: netGame(g) {
			}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				Debug::Print("Server Setup", Vector2(30, 30), Debug::WHITE);
				Debug::Print("Press 2 to Start 2-Player Game", Vector2(30, 45), Debug::WHITE);
				Debug::Print("Press ESC to Cancel", Vector2(30, 65), Debug::WHITE);

				int playerCount = 0;
				if (Window::GetKeyboard()->KeyPressed(KeyCodes::NUM2)) {
					playerCount = 2;
				}
				// we can expand more players here...
				// if we expand more players, we should also change the StartAsServer functions in NetworkedGame.cpp
				// start as server
				if (playerCount > 0) {
					netGame->StartAsServer(playerCount);

					*newState = new NetworkedGameState(netGame);
					return PushdownResult::Push;
				}

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::ESCAPE)) {
					return PushdownResult::Pop;
				}
				return PushdownResult::NoChange;
			}
		protected:
			NetworkedGame* netGame;
		};

		// Main menu state
		class MainMenuState : public PushdownState {
		public:
			MainMenuState(MyGame* g, NetworkedGame* ng, GameWorld* w, PhysicsSystem* p)
				: game(g), netGame(ng), world(w), physics(p) {
			}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				Debug::Print("CSC8503 Coursework", Vector2(35, 20), Debug::CYAN);
				Debug::Print("1. Start Single Player", Vector2(30, 40), Debug::WHITE);
				Debug::Print("2. Host Network Game (Server)", Vector2(30, 45), Debug::WHITE);
				Debug::Print("3. Join Network Game (Client)", Vector2(30, 50), Debug::WHITE);
				Debug::Print("Esc. Exit Game", Vector2(30, 60), Debug::WHITE);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::NUM1)) {
					*newState = new SinglePlayerState(game, world, physics, true);
					return PushdownResult::Push;
				}
				if (Window::GetKeyboard()->KeyPressed(KeyCodes::NUM2)) {
					*newState = new HostGameState(netGame);
					return PushdownResult::Push;
				}
				if (Window::GetKeyboard()->KeyPressed(KeyCodes::NUM3)) {
					*newState = new ClientGameState(netGame);
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