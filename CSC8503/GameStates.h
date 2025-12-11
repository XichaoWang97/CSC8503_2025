#pragma once
#include "PushdownState.h"
#include "MyGame.h"
#include "Window.h"
#include "Debug.h"

#include "PhysicsSystem.h"
#include "GameWorld.h"

#include "NetworkedGame.h"
#include "HighScoreManager.h"

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
				game->UpdateGame(dt); // it is necessary to update game here, make sure client can get the message after server win or lose
				Debug::Print("GameOver", Vector2(40, 40), Debug::RED);
				// different reasons print different words
				if (reason == GameOverReason::GooseCatch) {
					Debug::Print("Player Caught by Goose!", Vector2(25, 50), Debug::RED);
				}
				else if (reason == GameOverReason::RivalWin) {
					Debug::Print("The Rival Delivered the Package First!", Vector2(20, 50), Debug::RED);
					Debug::Print("You were too slow!", Vector2(35, 55), Debug::RED);
				}
				Debug::Print("Press ESC to Return to Menu", Vector2(30, 60), Debug::WHITE);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::ESCAPE)) {
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
			WinState(MyGame* g, bool networked = false) : game(g), isNetworked(networked) {
				finalTime = game->GetGameDuration();

				// Pass in isNetworked to check the corresponding leaderboard
				isNewRecord = HighScoreManager::Instance().IsNewRecord(finalTime, isNetworked);

				canInputName = !isNetworked || (isNetworked && ((NetworkedGame*)game)->GetServer() != nullptr);
				nameInput = "";
				hasSubmitted = false;
			}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				game->UpdateGame(dt);  // it is necessary to update game here, make sure client can get the message after server win or lose
				Debug::Print("MISSION SUCCESS!", Vector2(35, 30), Debug::GREEN);

				// Display Time
				std::string timeStr;
				game->FormatTime(finalTime, timeStr); // Assume FormatTime is public in MyGame
				Debug::Print("Time: " + timeStr, Vector2(40, 35), Debug::WHITE);

				if (isNewRecord) {
					Debug::Print("NEW RECORD!", Vector2(40, 25), Vector4(1, 1, 0, 1));

					if (canInputName && !hasSubmitted) {
						Debug::Print("Enter Name: " + nameInput + "_", Vector2(30, 50), Debug::CYAN);
						Debug::Print("Press ENTER to Submit", Vector2(30, 55), Debug::CYAN);

						HandleNameInput(); // Call input handling function

						if (Window::GetKeyboard()->KeyPressed(KeyCodes::RETURN)) {
							if (nameInput.empty()) nameInput = "Unknown";

							// Submit score
							HighScoreManager::Instance().AddScore(nameInput, finalTime, isNetworked);
							hasSubmitted = true;

							// If server, broadcast to others
							if (isNetworked) {
								((NetworkedGame*)game)->BroadcastHighScores();
							}
						}
						return PushdownResult::NoChange;
					}
					else if (!canInputName) {
						Debug::Print("You broke a record!", Vector2(30, 50), Debug::CYAN);
					}
				}

				Debug::Print("Press ESC to Return to Menu", Vector2(30, 70), Debug::WHITE);

				if (Window::GetKeyboard()->KeyDown(KeyCodes::TAB)) {
					game->DrawHighScoreHUD();
				}

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::ESCAPE)) {
					return PushdownResult::Pop;
				}

				return PushdownResult::NoChange;
			}

			// Input name
			void HandleNameInput() {

				const Keyboard* k = Window::GetKeyboard();

				// A-Z
				for (int i = 0; i < 26; ++i) {
					auto key = static_cast<decltype(KeyCodes::A)>((int)KeyCodes::A + i);

					if (k->KeyPressed(key)) {
						nameInput += (char)('A' + i);
					}
				}

				// Space
				if (k->KeyPressed(KeyCodes::SPACE)) {
					nameInput += ' ';
				}

				// Back
				if (k->KeyPressed(KeyCodes::BACK) && !nameInput.empty()) {
					nameInput.pop_back();
				}

				// Limit length
				if (nameInput.length() > 12) {
					nameInput.pop_back();
				}
			}

		protected:
			MyGame* game;
			float finalTime;
			bool isNewRecord;
			bool canInputName;
			bool hasSubmitted;
			bool isNetworked;
			std::string nameInput;
		};

		// Single player state
		class SinglePlayerState : public PushdownState {
		public:
			SinglePlayerState(MyGame* g, GameWorld* w, PhysicsSystem* p, bool reset = true)
				: game(g), world(w), physics(p) {
				gameFinished = false;
				if (reset) {
					game->ResetGame();
				}
			}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				if (gameFinished) {
					return PushdownResult::Pop;
				}

				game->UpdateGame(dt);

				// check game over conditions
				if (game->IsGameOver()) {
					*newState = new DeadState(game, game->GetGameOverReason());
					gameFinished = true;
					return PushdownResult::Push;
				}

				// check game win conditions
				if (game->IsGameWon()) {
					*newState = new WinState(game);
					gameFinished = true;
					return PushdownResult::Push;
				}

				Debug::Print("Press ESC to Pause", Vector2(5, 5), Debug::YELLOW);
				Debug::Print("Press 1 to Return to Main Menu", Vector2(5, 8), Debug::YELLOW);
				Debug::Print("Press 2 to show to Debug Volume", Vector2(5, 11), Debug::YELLOW);
				Debug::Print("Press TAB to Check Rank", Vector2(5, 14), Debug::YELLOW);
				Debug::Print("Press F1 to Restart Game", Vector2(5, 17), Debug::YELLOW);

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

		protected:
			MyGame* game;
			GameWorld* world;
			PhysicsSystem* physics;
			bool gameFinished;
		};

		// Networked game state
		class NetworkedGameState : public PushdownState {
		public:
			NetworkedGameState(NetworkedGame* g) : netGame(g) { gameFinished = false; }

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				netGame->UpdateGame(dt);

				// If just returned from result screen (gameFinished is true), disconnect and return to main menu
				if (gameFinished) {
					netGame->Disconnect(); // Clear network
					return PushdownResult::Pop; // Return to MainMenuState
				}

				Debug::Print("Multiplayer Mode", Vector2(5, 5), Debug::GREEN);
				Debug::Print("Press ESC to Disconnect", Vector2(5, 8), Debug::YELLOW);
				Debug::Print("Press TAB to Check Rank", Vector2(5, 11), Debug::YELLOW);

				// check game over conditions
				if (netGame->IsGameOver()) {
					*newState = new DeadState(netGame, netGame->GetGameOverReason());
					gameFinished = true;
					return PushdownResult::Push;
				}

				// check game win conditions
				if (netGame->IsGameWon()) {
					*newState = new WinState(netGame, true);
					gameFinished = true;
					return PushdownResult::Push;
				}

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::ESCAPE)) {
					netGame->Disconnect();
					return PushdownResult::Pop;
				}
				return PushdownResult::NoChange;
			}

		protected:
			NetworkedGame* netGame;
			bool gameFinished;
		};

		// Client game state
		class ClientGameState : public PushdownState {
		public:
			ClientGameState(NetworkedGame* g)
				: netGame(g) {
				// Do not create GameClient here, let NetworkedGame initialize it
				// Temporarily comment out auto-connect, change to key trigger in Update or call directly here
			}

			PushdownResult OnUpdate(float dt, PushdownState** newState) override {
				Debug::Print("Waiting to Connect...", Vector2(30, 40), Debug::WHITE);
				Debug::Print("Press SPACE to Connect Localhost", Vector2(30, 45), Debug::WHITE);
				Debug::Print("Press ESC to Return", Vector2(30, 50), Debug::WHITE);

				if (Window::GetKeyboard()->KeyPressed(KeyCodes::SPACE)) {
					// Call NetworkedGame method to start client
					netGame->StartAsClient(127, 0, 0, 1);

					// Enter game loop state
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
				Debug::Print("CSC8503 Coursework", Vector2(33, 20), Debug::CYAN);
				Debug::Print("1. Start Single Player", Vector2(30, 35), Debug::WHITE);
				Debug::Print("2. Host Network Game (Server)", Vector2(30, 40), Debug::WHITE);
				Debug::Print("3. Join Network Game (Client)", Vector2(30, 45), Debug::WHITE);
				Debug::Print("Esc. Exit Game", Vector2(30, 50), Debug::WHITE);

				// Controls Instruction
				Debug::Print("--- Controls ---", Vector2(35, 70), Debug::CYAN);
				Debug::Print("W/S: Move Fwd/Back", Vector2(5, 75), Debug::WHITE);
				Debug::Print("A/D: Turn Left/Right", Vector2(5, 80), Debug::WHITE);
				Debug::Print("Space: Jump", Vector2(5, 85), Debug::WHITE);
				Debug::Print("Mouse: Rotate Camera", Vector2(55, 75), Debug::WHITE);
				Debug::Print("LMB: Grab / Throw", Vector2(55, 80), Debug::WHITE);
				Debug::Print("TAB: Toggle Leaderboard", Vector2(55, 85), Debug::WHITE);

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

		protected:
			MyGame* game;
			NetworkedGame* netGame;
			GameWorld* world;
			PhysicsSystem* physics;
		};
	}
}