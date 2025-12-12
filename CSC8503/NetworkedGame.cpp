#include "NetworkedGame.h"
#include "NetworkPlayer.h"
#include "NetworkObject.h"
#include "GameServer.h"
#include "GameClient.h"
#include "GameWorld.h"
#include "Window.h"
#include "Matrix.h"
#define COLLISION_MSG 30
#define GLOBAL_STATE_MSG 50 
#define HIGHSCORE_MSG 60 

using namespace NCL;
using namespace CSC8503;

struct GlobalStatePacket : public GamePacket {
	int rivalScore;
	int playerScore; // score of player
	bool useGravity;

	// player states
	bool p2Connected;
	bool p1Dead;
	bool p2Dead;
	int  gameOverReason;

	// package states
	float packageHealth;
	bool  packageBroken;
	float packageTimer;

	int p1HeldItemID;
	int p2HeldItemID;

	GlobalStatePacket() {
		type = GLOBAL_STATE_MSG;
		size = sizeof(GlobalStatePacket) - sizeof(GamePacket);
		p2Connected = false;
		p1Dead = false;
		p2Dead = false;
		gameOverReason = 0;
		packageHealth = 100.0f;
		packageBroken = false;
		packageTimer = 0.0f;
		p1HeldItemID = -1; // -1 means nothing held
		p2HeldItemID = -1;
	}
};

struct MessagePacket : public GamePacket {
	short playerID;
	short messageID;

	MessagePacket() {
		type = Message;
		size = sizeof(short) * 2;
	}
};

struct HighScorePacket : public GamePacket {
	int count;
	ScoreEntry entries[5];

	HighScorePacket() {
		type = HIGHSCORE_MSG;
		size = sizeof(HighScorePacket) - sizeof(GamePacket);
		count = 0;
	}
};

NetworkedGame::NetworkedGame(GameWorld& gameWorld, GameTechRendererInterface& renderer, PhysicsSystem& physics) : MyGame(gameWorld, renderer, physics)
{
	thisServer = nullptr;
	thisClient = nullptr;
	NetworkBase::Initialise();
	timeToNextPacket = 0.0f;
	packetsToSnapshot = 0;
	isNetworkGame = true;
}

NetworkedGame::~NetworkedGame() {
	delete thisServer;
	delete thisClient;
}

void NetworkedGame::StartAsServer(int playerCount) {
	// Force reset local network state flags
	ui_p1Dead = false;
	ui_p2Dead = false;
	ui_p2Connected = false;
	isP2ConnectedServer = false;
	gameOverReason = GameOverReason::None;
	isGameOver = false;
	isGameWon = false;
	timeSinceLastP2Packet = 0.0f;
	stateIDs.clear(); // Clear old packet acknowledgement records
	Disconnect();

	thisServer = new GameServer(NetworkBase::GetDefaultPort(), 1); // if you need more clients, change the number here
	// Register handlers: Input packets from client + Acknowledgement packets
	thisServer->RegisterPacketHandler(BasicNetworkMessages::Client_Update, this);
	thisServer->RegisterPacketHandler(BasicNetworkMessages::Received_State, this);
	thisServer->RegisterPacketHandler(BasicNetworkMessages::Player_Connected, this);
	thisServer->RegisterPacketHandler(BasicNetworkMessages::Player_Disconnected, this);

	InitWorld();
	InitNetworkObjectToWorld();

	isP2ConnectedServer = false;
	// generate players
	for (int i = 0; i < playerCount; ++i) {
		GameObject* newPlayer = SpawnNetworkedPlayer(i);

		// Record to Server's Map for easy lookup via NetworkID
		serverPlayers[i] = newPlayer;
	}
	localPlayerID = 0; // server ID is 0 

	// Allow host player (ID 0) to read local keyboard directly, bypassing the 20Hz network limit, ensuring response every frame at 60fps
	Player* localPlayer = (Player*)serverPlayers[localPlayerID];
	if (localPlayer) {
		localPlayer->SetIgnoreInput(false);
	}

	// tell AI where is player
	if (goose) goose->SetPlayerList(&players);
	if (rival) rival->SetPlayerList(&players);
	for (auto* enemy : patrolEnemy) {
		if (enemy) enemy->SetPlayerList(&players);
	}
}

void NetworkedGame::StartAsClient(char a, char b, char c, char d) {
	// Force reset local network state flags
	ui_p1Dead = false;
	ui_p2Dead = false;
	ui_p2Connected = false;
	isP2ConnectedServer = false;
	gameOverReason = GameOverReason::None;
	isGameOver = false;
	isGameWon = false;
	stateIDs.clear(); // Clear old packet acknowledgement records

	Disconnect();
	thisClient = new GameClient();
	if (thisClient->Connect(a, b, c, d, NetworkBase::GetDefaultPort())) {
		std::cout << "Connected to Server!" << std::endl;
	}

	// register messages
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Delta_State, this);
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Full_State, this);
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Player_Connected, this);
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Player_Disconnected, this);
	thisClient->RegisterPacketHandler(GLOBAL_STATE_MSG, this);
	thisClient->RegisterPacketHandler(HIGHSCORE_MSG, this);

	InitWorld();
	InitNetworkObjectToWorld();

	localPlayerID = 1;
	int estimatedPlayers = 2;

	// no physics for everything
	for (int i = 0; i < estimatedPlayers; ++i) {
		GameObject* p = SpawnNetworkedPlayer(i);
		if (i != localPlayerID) {
			p->GetPhysicsObject()->SetInverseMass(0.0f);
		}
	}

	if (goose && goose->GetPhysicsObject()) goose->GetPhysicsObject()->SetInverseMass(0.0f);
	if (rival && rival->GetPhysicsObject()) rival->GetPhysicsObject()->SetInverseMass(0.0f);
	if (packageObject && packageObject->GetPhysicsObject()) packageObject->GetPhysicsObject()->SetInverseMass(0.0f);
	world.OperateOnContents([&](GameObject* o) {
		if (o->GetName() == "Stone" || o->GetName() == "CubeStone" || o->GetName() == "enemy") {
			o->GetPhysicsObject()->SetInverseMass(0.0f);
		}
		});

	// Tell the client side AI where the player list is (although they are mainly for visual representation)
	if (goose) goose->SetPlayerList(&players);
	if (rival) rival->SetPlayerList(&players);
	for (auto* enemy : patrolEnemy) {
		if (enemy) enemy->SetPlayerList(&players);
	}
}

void NetworkedGame::UpdateGame(float dt) {
	if (!isGameOver && !isGameWon) {
		MyGame::UpdateGame(dt);
	}

	// Client logic: Run every frame to ensure no keystrokes are missed
	if (thisClient) {
		UpdateAsClient(dt);
	}

	// Send packets to client
	timeToNextPacket -= dt;
	if (timeToNextPacket <= 0.0f) {
		if (thisServer) {
			UpdateAsServer(dt);
		}
		timeToNextPacket += 1.0f / 60.0f; // improve to 60Hz
	}

	// Network update
	if (thisServer) thisServer->UpdateServer();
	if (thisClient) thisClient->UpdateClient();
	DrawNetworkHUD();
}

void NetworkedGame::UpdateAsServer(float dt) {
	// Handle timeout logic
	if (isP2ConnectedServer) {
		timeSinceLastP2Packet += dt;
		// If no message from P2 for over 2.0 seconds, force disconnect
		if (timeSinceLastP2Packet > 2.0f) {
			std::cout << "Player 2 Timed Out (Heartbeat lost)!" << std::endl;
			isP2ConnectedServer = false;
		}
	}

	// Cooperative Package Mechanics
	if (packageObject && packageObject->GetPhysicsObject()) {
		// Determine how many players are currently Online
		int requiredGrabbers = 1; // Player 1 is always online (Host)
		if (isP2ConnectedServer) {
			requiredGrabbers = 2; // If P2 is connected, we need 2 people
		}

		// Count how many players are currently holding the package
		int currentGrabbers = 0;
		// Check Host (Player 0)
		if (!players.empty() && players[0]->GetHeldItem() == packageObject) {
			currentGrabbers++;
		}
		// Check Client (Player 1) - Only if connected
		if (players.size() > 1 && isP2ConnectedServer) {
			if (players[1]->GetHeldItem() == packageObject) {
				currentGrabbers++;
			}
		}

		// Check if Rival is holding it (Rival is strong, doesn't need help)
		bool rivalHasIt = (rival && rival->GetHeldItem() == packageObject);

		// Apply Physics State
		if (rivalHasIt || currentGrabbers >= requiredGrabbers) {
			// Make it movable (Normal Mass)
			if (packageObject->GetPhysicsObject()->GetInverseMass() == 0.0f) {
				packageObject->GetPhysicsObject()->SetInverseMass(1.0f);
			}
		}
		else {
			// Make it immovable (Infinite Mass)
			// This allows players to grab it (attach constraint), but they can't pull it until the count is met
			packageObject->GetPhysicsObject()->SetInverseMass(0.0f);
			// Stop existing momentum so it doesn't slide if someone lets go
			packageObject->GetPhysicsObject()->SetLinearVelocity(Vector3(0, 0, 0));
			packageObject->GetPhysicsObject()->SetAngularVelocity(Vector3(0, 0, 0));
		}
	}

	// Just take the results calculated by MyGame to send packets
	GlobalStatePacket statePacket;
	statePacket.useGravity = useGravity;
	// Get the NetworkID of the item held by the player
	statePacket.p1HeldItemID = -1;
	statePacket.p2HeldItemID = -1;
	// Check Player 1 (Host)
	if (!players.empty() && players[0]->GetHeldItem()) {
		NetworkObject* no = players[0]->GetHeldItem()->GetNetworkObject();
		if (no) statePacket.p1HeldItemID = no->GetNetworkID();
	}

	// Check Player 2 (Client)
	if (players.size() > 1 && players[1]->GetHeldItem()) {
		NetworkObject* no = players[1]->GetHeldItem()->GetNetworkObject();
		if (no) statePacket.p2HeldItemID = no->GetNetworkID();
	}

	// Sync score
	if (rival) statePacket.rivalScore = rival->GetScore();

	int currentPackageScore = 0;
	if (packageObject) currentPackageScore = packageObject->GetCollectionCount();
	statePacket.playerScore = 0;
	// Check if player is holding the package
	if (!players.empty() && players[0]->GetHeldItem() == packageObject ||
		players.size() > 1 && players[1]->GetHeldItem() == packageObject) {
		statePacket.playerScore = currentPackageScore;
	}

	// Read calculation results directly from MyGame
	statePacket.gameOverReason = (int)this->gameOverReason; // Calculated by MyGame::WinLoseLogic
	statePacket.p2Connected = isP2ConnectedServer;

	// Sync package state
	if (packageObject) {
		statePacket.packageHealth = packageObject->GetHealth();
		statePacket.packageBroken = packageObject->IsBroken();
	}

	// Get player death state (for UI)
	// This step is a bit redundant, but kept for convenience in packet sending, or could read players[i]->IsDead() directly
	statePacket.p1Dead = false;
	statePacket.p2Dead = false;
	if (!players.empty()) statePacket.p1Dead = players[0]->IsDead();
	if (players.size() > 1) statePacket.p2Dead = players[1]->IsDead();

	thisServer->SendGlobalPacket(statePacket);

	// Snapshot logic remains unchanged
	packetsToSnapshot--;
	if (packetsToSnapshot < 0) {
		BroadcastSnapshot(false);
		packetsToSnapshot = 5;
	}
	else {
		BroadcastSnapshot(true);
	}

}

void NetworkedGame::BroadcastSnapshot(bool deltaFrame) {
	std::vector<GameObject*>::const_iterator first;
	std::vector<GameObject*>::const_iterator last;

	world.GetObjectIterators(first, last);

	for (auto i = first; i != last; ++i) {
		NetworkObject* o = (*i)->GetNetworkObject();
		if (!o) continue;
		int playerState = 0;
		GamePacket* newPacket = nullptr;
		if (o->WritePacket(&newPacket, deltaFrame, playerState)) {
			thisServer->SendGlobalPacket(*newPacket);
			delete newPacket;
		}
	}
}

void NetworkedGame::ReceivePacket(int type, GamePacket* payload, int source) {
	if (thisServer) {
		// Reset timer as long as a packet is received from Client (source is usually 0 for the first client)
		if (source == 0) {
			timeSinceLastP2Packet = 0.0f;
		}

		BroadcastHighScores(); // send ranks
		switch (type) {
		case BasicNetworkMessages::Client_Update: {
			// Server gets input of Client -> control relative Player
			int playerIndex = source + 1;

			ServerProcessClientInput(playerIndex, (ClientPacket*)payload);
			break;
		}
		case BasicNetworkMessages::Player_Connected: { // p2 connected
			isP2ConnectedServer = true;
			BroadcastHighScores();
			break;
		}
		case BasicNetworkMessages::Player_Disconnected: { // p2 disconnected
			isP2ConnectedServer = false;
			std::cout << "Player 2 Disconnected!" << std::endl;
			break;
		}
		case BasicNetworkMessages::Received_State: {
			break; // handle received-state ack if used
		}
		}
	}
	else if (thisClient) {
		// deal with rank
		if (type == HIGHSCORE_MSG) {
			HighScorePacket* pkt = (HighScorePacket*)payload;
			// Update local rank
			auto& list = HighScoreManager::Instance().GetScores(true);
			list.clear();
			for (int i = 0; i < pkt->count; ++i) {
				list.push_back(pkt->entries[i]);
			}
		}

		switch (type) { // IMPORTANT, update position of objects in the whole world (must have networked ID)!!!
			// Receive Full State (Full_State) or Delta State (Delta_State)
		case BasicNetworkMessages::Full_State:
		case BasicNetworkMessages::Delta_State: {
			int objectID = -1;
			// Parse object ID from network packet
			if (type == Full_State) objectID = ((FullPacket*)payload)->objectID;
			else objectID = ((DeltaPacket*)payload)->objectID;

			// Iterate through all objects in the local game world
			std::vector<GameObject*>::const_iterator first;
			std::vector<GameObject*>::const_iterator last;
			world.GetObjectIterators(first, last);
			for (auto i = first; i != last; ++i) {
				NetworkObject* o = (*i)->GetNetworkObject();
				// Find object matching NetworkID
				if (o && o->GetNetworkID() == objectID) {
					// Call ReadPacket to read position and rotation data and apply to the object
					o->ReadPacket(*payload);
					break;
				}
			}
			break;
		}
		}
	}
	if (type == GLOBAL_STATE_MSG) {
		GlobalStatePacket* packet = (GlobalStatePacket*)payload;

		// Original sync
		if (this->useGravity != packet->useGravity) {
			this->useGravity = packet->useGravity;
			physics.UseGravity(this->useGravity);
		}
		if (rival) rival->SetScore(packet->rivalScore);
		this->score = packet->playerScore;
		// Client handles package logic
		if (packageObject) {
			// Sync health (for correct UI display)
			packageObject->SetHealth(packet->packageHealth);

			// Handle fragmentation logic
			if (packet->packageBroken) {
				// Set local object state
				packageObject->SetBroken(true); // If this function doesn't exist, need to add it to Package

				// Force the holder to let go!
				// If not released, the client will keep trying to pull the package back, causing visual glitches
				for (Player* p : players) {
					if (p->GetHeldItem() == packageObject) {
						p->ThrowHeldItem(Vector3(0, 0, 0)); // Position sync will only be smooth after letting go
					}
				}
			}
		}
		// Sync visual effect of grapple line
		if (thisClient) { // Only client needs to sync this, server already knows

			// Define a lambda function to find object by ID
			auto FindObjByID = [&](int id) -> GameObject* {
				if (id == -1) return nullptr;
				std::vector<GameObject*>::const_iterator first;
				std::vector<GameObject*>::const_iterator last;
				world.GetObjectIterators(first, last);
				for (auto i = first; i != last; ++i) {
					if ((*i)->GetNetworkObject() && (*i)->GetNetworkObject()->GetNetworkID() == id) {
						return (*i);
					}
				}
				return nullptr;
				};

			// Handle Player 1 (Server Player)
			if (!players.empty()) {
				GameObject* heldItem = FindObjByID(packet->p1HeldItemID);
				// Call the new function we wrote in step 1
				players[0]->SetHeldItemNetwork(heldItem);
			}

			// Handle Player 2 (Self/Client Player)
			// Although there is local prediction, overwriting with server-confirmed data is more accurate
			if (players.size() > 1) {
				GameObject* heldItem = FindObjByID(packet->p2HeldItemID);
				players[1]->SetHeldItemNetwork(heldItem);
			}
		}
		// Sync game over reason
		GameOverReason serverReason = (GameOverReason)packet->gameOverReason;
		if (!this->isGameWon && !this->isGameOver) {
			if (serverReason == GameOverReason::PlayerWin) {
				this->isGameWon = true;
				this->isGameOver = false;
				this->gameOverReason = serverReason;
			}
			else if (serverReason != GameOverReason::None) {
				this->isGameOver = true;
				this->isGameWon = false;
				this->gameOverReason = serverReason;
			}
			else {
				this->isGameOver = false;
				this->isGameWon = false;
			}
		}

		// Sync player death state and connection state
		// Save for UI rendering
		ui_p1Dead = packet->p1Dead;
		ui_p2Dead = packet->p2Dead;
		ui_p2Connected = packet->p2Connected;

		// Update local Player object state (optional, to prevent the corpse from moving)
		if (!players.empty()) players[0]->SetDead(ui_p1Dead);
		if (players.size() > 1) players[1]->SetDead(ui_p2Dead);
	}
}

void NetworkedGame::ServerProcessClientInput(int playerID, ClientPacket* packet) {
	// Find corresponding player object
	if (playerID < 0 || playerID >= players.size()) return;

	// Cast to Player* to call SetPlayerInput
	Player* playerObj = (Player*)players[playerID];
	if (!playerObj) return;

	// Construct input state
	PlayerInputs inputs;

	// Parse axis
	inputs.axis = Vector3(0, 0, 0);
	if (packet->axis[0] != 0) inputs.axis.z = (float)packet->axis[0] / 100.0f;
	if (packet->axis[1] != 0) inputs.axis.x = (float)packet->axis[1] / 100.0f;

	if (Vector::LengthSquared(inputs.axis) > 0) inputs.isMoving = true;

	// Parse camera angle
	inputs.cameraYaw = packet->yaw;

	// Parse keys
	if (packet->buttonstates[0] > 0) inputs.jump = true;
	if (packet->buttonstates[1] > 0) inputs.attack = true;

	// [Critical] Convert network packet to Player input
	playerObj->SetPlayerInput(inputs);
}

// NetworkedGame.cpp

void NetworkedGame::UpdateAsClient(float dt) {
	ClientPacket newPacket;
	int forward = 0;
	int right = 0;

	// Collect input for packet sending (This part remains unchanged, sent to server for movement)
	if (Window::GetKeyboard()->KeyDown(KeyCodes::W)) forward -= 100;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::S)) forward += 100;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::A)) right -= 100;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::D)) right += 100;

	newPacket.axis[0] = forward;
	newPacket.axis[1] = right;
	newPacket.yaw = world.GetMainCamera().GetYaw();
	newPacket.lastID = 0;
	newPacket.buttonstates[0] = 0;
	newPacket.buttonstates[1] = 0;

	// Jump input: Trigger on KeyPressed + keep sending for a duration
	static float jumpSendTimer = 0.0f;
	// Start send timer when Space is pressed
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::SPACE)) {
		jumpSendTimer = 0.2f; // Send for 200ms (approx 12 frames @ 60Hz)
	}
	// If player releases key, stop sending immediately
	if (!Window::GetKeyboard()->KeyDown(KeyCodes::SPACE)) {
		jumpSendTimer = 0.0f;
	}
	// Keep sending while timer is active
	if (jumpSendTimer > 0.0f) {
		newPacket.buttonstates[0] = 1;
		jumpSendTimer -= dt;
	}

	if (Window::GetMouse()->ButtonPressed(MouseButtons::Left)) newPacket.buttonstates[1] = 1;



	// --- Modification Start: Local Logic ---

	/*if (localPlayerID >= 0 && localPlayerID < players.size()) {
		Player* localPlayer = players[localPlayerID];
		if (localPlayer) {
			PlayerInputs inputs;

			// Force movement axis to 0!
			// This way "if (currentInputs.isMoving)" in Player::PlayerControl will skip physics force application
			inputs.axis = Vector3(0, 0, 0);
			inputs.isMoving = false; // Explicitly tell local logic: Do not move!

			localPlayer->SetIgnoreInput(false);
			localPlayer->SetPlayerInput(inputs);
		}
	}*/
	// --- Modification End ---

	// check connection every second (keep unchanged)
	static float connectTimer = 0.0f;
	connectTimer += dt;
	if (connectTimer > 1.0f) {
		GamePacket connectPacket;
		connectPacket.type = BasicNetworkMessages::Player_Connected;
		connectPacket.size = 0;
		thisClient->SendPacket(connectPacket);
		connectTimer = 0.0f;
	}

	thisClient->SendPacket(newPacket); // Send packet to server
}

GameObject* NetworkedGame::SpawnNetworkedPlayer(int playerID) {
	// Calculate spawn position, stand in a row
	Vector3 pos = Vector3(0 + (playerID * 10), 20, 170);
	Player* newPlayer = AddPlayerToWorld(pos, 1.0f);
	// Set NetworkObject, NetworkID = playerID
	newPlayer->SetNetworkObject(new NetworkObject(*newPlayer, playerID));
	// Save to MyGame's vector
	players.push_back(newPlayer);

	newPlayer->SetIgnoreInput(true);
	return newPlayer;
}

void NetworkedGame::OnPlayerCollision(NetworkPlayer* a, NetworkPlayer* b) {
	if (thisServer) { //detected a collision between players!
		MessagePacket newPacket;
		newPacket.messageID = COLLISION_MSG;
		newPacket.playerID = a->GetPlayerNum();

		thisClient->SendPacket(newPacket);

		newPacket.playerID = b->GetPlayerNum();
		thisClient->SendPacket(newPacket);
	}
}

void NetworkedGame::UpdateMinimumState() {
	//Periodically remove old data from the server
	int minID = INT_MAX;
	int maxID = 0; //we could use this to see if a player is lagging behind?

	for (auto i : stateIDs) {
		minID = std::min(minID, i.second);
		maxID = std::max(maxID, i.second);
	}
	//every client has acknowledged reaching at least state minID
	//so we can get rid of any old states!
	std::vector<GameObject*>::const_iterator first;
	std::vector<GameObject*>::const_iterator last;
	world.GetObjectIterators(first, last);

	for (auto i = first; i != last; ++i) {
		NetworkObject* o = (*i)->GetNetworkObject();
		if (!o) {
			continue;
		}
		o->UpdateStateHistory(minID); //clear out old states so they arent taking up memory...
	}
}

// Auxiliary Functions-----------------------------~o(> v < )o

void NetworkedGame::DrawNetworkHUD() {
	float startX = 80.0f; // Right side of screen
	float startY = 25.0f;

	// Draw Player 1
	Vector4 p1Color = Vector4(0, 1, 0, 1); // green name means alive

	if (ui_p1Dead) {
		p1Color = Vector4(1, 0, 0, 1); // dead is red name
	}

	Debug::Print("Player 1", Vector2(startX, startY), p1Color);

	// Draw Player 2
	Vector4 p2Color = Vector4(0.5f, 0.5f, 0.5f, 1); // grey means not connected
	std::string p2Name = "Player 2";

	bool isP2Online = (thisServer) ? isP2ConnectedServer : ui_p2Connected;

	if (isP2Online) {
		p2Color = Vector4(0, 1, 0, 1); // green

		if (ui_p2Dead) {
			p2Color = Vector4(1, 0, 0, 1); // red
		}
	}

	Debug::Print(p2Name, Vector2(startX, startY + 5), p2Color);
}

void NetworkedGame::Disconnect() {
	if (thisServer) {
		delete thisServer;
		thisServer = nullptr;
	}
	if (thisClient) {
		delete thisClient; // I added netHandle = nullptr in the deconstruction function in GameClient.cpp
		// Otherwise, there should be no delete here, it will cause BUG! (Double Free)
		thisClient = nullptr;
	}
	localPlayerID = -1;
}

void NetworkedGame::BroadcastHighScores() {
	if (!thisServer) return;

	HighScorePacket packet;
	// get rank data
	auto& scores = HighScoreManager::Instance().GetScores(true);

	packet.count = (int)scores.size();
	for (int i = 0; i < packet.count; ++i) {
		packet.entries[i] = scores[i];
	}
	thisServer->SendGlobalPacket(packet);
}

// This is an empty function to disable the default single-player player creation
void NetworkedGame::InitDefaultPlayer() {
	// Do Nothing.
}

void NetworkedGame::InitNetworkObjectToWorld() {
	// we assume 0-4 to player
	// start to allocate from 5
	if (rival) rival->SetNetworkObject(new NetworkObject(*rival, 5));
	if (goose) goose->SetNetworkObject(new NetworkObject(*goose, 6));
	if (packageObject) packageObject->SetNetworkObject(new NetworkObject(*packageObject, 7));

	int idCounter = 10;
	// allocate numbers to coins
	for (auto& coin : coins) {
		if (!coin->GetNetworkObject()) {
			coin->SetNetworkObject(new NetworkObject(*coin, idCounter++));
		}
	}
	// allocate enemies
	for (auto* enemy : patrolEnemy) {
		enemy->SetNetworkObject(new NetworkObject(*enemy, idCounter++));
	}
	// allocate stones and cube stones
	world.OperateOnContents([&](GameObject* o) {
		// check objects by name
		if (o->GetName() == "Stone" || o->GetName() == "CubeStone") {
			if (!o->GetNetworkObject()) {
				o->SetNetworkObject(new NetworkObject(*o, idCounter++));
			}
		}
		});
}

// Override Functions Below-----------------------------~o(> v < )o

void NetworkedGame::PackageLogic(Player* player, float dt) {
	if (thisServer) {
		// calculate
		MyGame::PackageLogic(player, dt);
	}
	else if (thisClient) {
		// update
		if (packageObject) {
			packageObject->Update(dt);
		}
	}
}

void NetworkedGame::RivalLogic() {
	if (thisServer) {
		MyGame::RivalLogic();
	}
	else {
		return;
	}
}

void NetworkedGame::WinLoseLogic(Player* player) {
	if (thisServer) {
		MyGame::WinLoseLogic(player);
	}
	else {
		return;
	}
}

void NetworkedGame::GetCoinLogic(Player* player, float dt) {
	if (thisServer) {
		MyGame::GetCoinLogic(player, dt);
	}
	else {
		return;
	}
}

void NetworkedGame::UpdateKeys() {
	// Only about gravity
	if (thisServer) {
		if (Window::GetKeyboard()->KeyPressed(KeyCodes::G)) {
			useGravity = !useGravity;
			physics.UseGravity(useGravity);

			GlobalStatePacket statePacket;
			statePacket.useGravity = useGravity;

			thisServer->SendGlobalPacket(statePacket);
		}
	}
	else if (thisClient) {
		// Set here empty
	}
	else {
		MyGame::UpdateKeys();
	}
}