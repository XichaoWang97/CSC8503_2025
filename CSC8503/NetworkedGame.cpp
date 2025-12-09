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

	// states
	bool p2Connected;
	bool p1Dead;
	bool p2Dead;
	int  gameOverReason;

	GlobalStatePacket() {
		type = GLOBAL_STATE_MSG;
		size = sizeof(GlobalStatePacket) - sizeof(GamePacket);
		p2Connected = false;
		p1Dead = false;
		p2Dead = false;
		gameOverReason = 0;
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
	// 1. 【新增】强制重置本地网络状态标记
	ui_p1Dead = false;
	ui_p2Dead = false;
	ui_p2Connected = false;
	isP2ConnectedServer = false;
	gameOverReason = GameOverReason::None;
	isGameOver = false;
	isGameWon = false;
	stateIDs.clear(); // 清空旧的包确认记录
	Disconnect();

	thisServer = new GameServer(NetworkBase::GetDefaultPort(), 1); // if you need more clients, change the number here
	// 注册处理: 客户端发来的输入包 + 确认包
	thisServer->RegisterPacketHandler(BasicNetworkMessages::Client_Update, this);
	thisServer->RegisterPacketHandler(BasicNetworkMessages::Received_State, this);
	thisServer->RegisterPacketHandler(BasicNetworkMessages::Player_Connected, this);

	InitWorld();
	InitNetworkObjectToWorld();

	isP2ConnectedServer = false;
	// generate players
	for (int i = 0; i < playerCount; ++i) {
		GameObject* newPlayer = SpawnNetworkedPlayer(i);

		// 记录到 Server 的 Map 中，方便通过 NetworkID 查找
		serverPlayers[i] = newPlayer;
	}
	localPlayerID = 0; // server ID is 0 

	// 允许主机玩家(ID 0)直接读取本地键盘，不再受 20Hz 网络频率限制, 这样 60fps 下每一帧都能响应操作
	Player* localPlayer = (Player*)serverPlayers[localPlayerID];
	if (localPlayer) {
		localPlayer->SetIgnoreInput(false);
	}

	// tell AI where is player
	if (goose) goose->SetPlayerList(&players);
	if (rival) rival->SetPlayerList(&players);
	if (patrolEnemy) patrolEnemy->SetPlayerList(&players);
}

void NetworkedGame::StartAsClient(char a, char b, char c, char d) {
	// 1. 【新增】强制重置本地网络状态标记
	ui_p1Dead = false;
	ui_p2Dead = false;
	ui_p2Connected = false;
	isP2ConnectedServer = false;
	gameOverReason = GameOverReason::None;
	isGameOver = false;
	isGameWon = false;
	stateIDs.clear(); // 清空旧的包确认记录

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
	// 【核心逻辑】客户端也初始化容器, 根据数字初始化 Vector。
	int estimatedPlayers = 2;

	for (int i = 0; i < estimatedPlayers; ++i) {
		GameObject* p = SpawnNetworkedPlayer(i);
		if (i != localPlayerID) {
			p->GetPhysicsObject()->SetInverseMass(0.0f); // no physics
		}
	}
	
	// 处理 Goose/AI 的本地物理禁用
	if (goose && goose->GetPhysicsObject()) goose->GetPhysicsObject()->SetInverseMass(0.0f);
	if (rival && rival->GetPhysicsObject()) rival->GetPhysicsObject()->SetInverseMass(0.0f);

	// 【新增修复代码】告诉客户端的 AI 玩家列表在哪里（虽然它们主要是视觉表现）
	if (goose) goose->SetPlayerList(&players);
	if (rival) rival->SetPlayerList(&players);
	if (patrolEnemy) patrolEnemy->SetPlayerList(&players);
}

void NetworkedGame::UpdateGame(float dt) {
	if (!isGameOver && !isGameWon) {
		MyGame::UpdateGame(dt);
	}

	// 客户端逻辑：每帧都运行，确保不漏掉按键
	if (thisClient) {
		UpdateAsClient(dt);
	}

	// 给客户端发包
	timeToNextPacket -= dt;
	if (timeToNextPacket <= 0.0f) {
		if (thisServer) {
			UpdateAsServer(dt);
		}
		timeToNextPacket += 1.0f / 60.0f; // improve to 60Hz
	}

	// 3. 网络更新
	if (thisServer) thisServer->UpdateServer();
	if (thisClient) thisClient->UpdateClient();

	DrawNetworkHUD();
}

// Server Logic
void NetworkedGame::UpdateAsServer(float dt) {
	// 3. 只需要把 MyGame 算好的结果拿来发包
	GlobalStatePacket statePacket;
	statePacket.useGravity = useGravity;

	// 同步分数
	if (rival) statePacket.rivalScore = rival->GetScore();

	int currentPackageScore = 0;
	if (packageObject) currentPackageScore = packageObject->GetCollectionCount();
	statePacket.playerScore = 0;
	// 检查 player 是否拿着包
	if (!players.empty() && players[0]->GetHeldItem() == packageObject || 
		players.size() > 1 && players[1]->GetHeldItem() == packageObject) {
		statePacket.playerScore = currentPackageScore;
	}

	// 直接读取 MyGame 的计算结果
	statePacket.gameOverReason = (int)this->gameOverReason; // MyGame::WinLoseLogic 算出来的
	statePacket.p2Connected = isP2ConnectedServer;

	// 获取玩家死亡状态 (用于UI)
	// 这一步虽然有点冗余，但为了发包方便可以保留，或者直接去读 players[i]->IsDead()
	statePacket.p1Dead = false;
	statePacket.p2Dead = false;
	if (!players.empty()) statePacket.p1Dead = players[0]->IsDead();
	if (players.size() > 1) statePacket.p2Dead = players[1]->IsDead();

	thisServer->SendGlobalPacket(statePacket);

	// Snapshot 逻辑保持不变
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
		std::cout << "Server sending packet for object ID: " << o->GetNetworkID() << std::endl;
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
		// 接收全量状态 (Full_State) 或 增量状态 (Delta_State)
		case BasicNetworkMessages::Full_State:
		case BasicNetworkMessages::Delta_State: {
			int objectID = -1;
			// 1. 解析网络包中的物体 ID
			if (type == Full_State) objectID = ((FullPacket*)payload)->objectID;
			else objectID = ((DeltaPacket*)payload)->objectID;

			// 2. 遍历本地游戏世界中的所有物体
			std::vector<GameObject*>::const_iterator first;
			std::vector<GameObject*>::const_iterator last;
			world.GetObjectIterators(first, last);
			for (auto i = first; i != last; ++i) {
				NetworkObject* o = (*i)->GetNetworkObject();
				// 3. 找到 NetworkID 匹配的物体
				if (o && o->GetNetworkID() == objectID) {
					// 4. 【关键步骤】调用 ReadPacket 读取位置和旋转数据并应用到物体上
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

		// 原有同步
		if (this->useGravity != packet->useGravity) {
			this->useGravity = packet->useGravity;
			physics.UseGravity(this->useGravity);
		}
		if (rival) rival->SetScore(packet->rivalScore);
		this->score = packet->playerScore;
		// 同步游戏结束原因
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

		// [新增] 同步玩家死亡状态与连接状态
		// 存下来用于 UI 绘制
		ui_p1Dead = packet->p1Dead;
		ui_p2Dead = packet->p2Dead;
		ui_p2Connected = packet->p2Connected;

		// 更新本地 Player 对象状态 (可选，为了防止尸体还能动)
		if (!players.empty()) players[0]->SetDead(ui_p1Dead);
		if (players.size() > 1) players[1]->SetDead(ui_p2Dead);
	}
}

void NetworkedGame::ServerProcessClientInput(int playerID, ClientPacket* packet) {
	// 查找对应的玩家对象
	if (playerID < 0 || playerID >= players.size()) return;

	// 强转为 Player* 以便调用 SetPlayerInput
	Player* playerObj = (Player*)players[playerID];
	if (!playerObj) return;

	// 构造输入状态
	PlayerInputs inputs;

	// 解析轴向
	inputs.axis = Vector3(0, 0, 0);
	if (packet->axis[0] != 0) inputs.axis.z = (float)packet->axis[0] / 100.0f;
	if (packet->axis[1] != 0) inputs.axis.x = (float)packet->axis[1] / 100.0f;

	if (Vector::LengthSquared(inputs.axis) > 0) inputs.isMoving = true;

	// 解析摄像机角度
	inputs.cameraYaw = packet->yaw;

	// 解析按键
	if (packet->buttonstates[0] > 0) inputs.jump = true;
	if (packet->buttonstates[1] > 0) inputs.attack = true;

	// 【关键】将网络包转化为 Player 的输入
	playerObj->SetPlayerInput(inputs);
}

// Client Logic
void NetworkedGame::UpdateAsClient(float dt) {
	ClientPacket newPacket;
	int forward = 0;
	int right = 0;

	if (Window::GetKeyboard()->KeyDown(KeyCodes::W)) forward -= 100; // Z-
	if (Window::GetKeyboard()->KeyDown(KeyCodes::S)) forward += 100; // Z+
	if (Window::GetKeyboard()->KeyDown(KeyCodes::A)) right -= 100;   // X-
	if (Window::GetKeyboard()->KeyDown(KeyCodes::D)) right += 100;   // X+

	newPacket.axis[0] = forward;
	newPacket.axis[1] = right;

	newPacket.yaw = world.GetMainCamera().GetYaw();

	newPacket.lastID = 0;

	newPacket.buttonstates[0] = 0;
	newPacket.buttonstates[1] = 0;
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::SPACE)) newPacket.buttonstates[0] = 1;
	if (Window::GetMouse()->ButtonPressed(MouseButtons::Left)) newPacket.buttonstates[1] = 1;


	// 核心逻辑：除了发包给服务器，也要把这个输入应用到本地的 Player 身上，
	// 这样本地 Player 才会执行射线检测和 DrawLine。
	// 1. 获取本地玩家对象
	if (localPlayerID >= 0 && localPlayerID < players.size()) {
		Player* localPlayer = players[localPlayerID];

		if (localPlayer) {
			// 2. 构造本地 Input 结构 (模仿 ServerProcessClientInput 的逻辑)
			PlayerInputs inputs;

			// 设置朝向 (射线方向依赖于此)
			inputs.cameraYaw = world.GetMainCamera().GetYaw();

			// 设置按键状态
			if (newPacket.buttonstates[0] > 0) inputs.jump = true;
			if (newPacket.buttonstates[1] > 0) inputs.attack = true; // 主要是这个触发交互/射线

			// deal with inputs
			bool wasIgnoring = localPlayer->GetIgnoreInput();
			localPlayer->SetIgnoreInput(false);
			localPlayer->SetPlayerInput(inputs);
		}
	}
	// check connection every second
	static float connectTimer = 0.0f;
	connectTimer += dt;
	if (connectTimer > 1.0f) {
		GamePacket connectPacket;
		connectPacket.type = BasicNetworkMessages::Player_Connected;
		connectPacket.size = 0;
		thisClient->SendPacket(connectPacket);
		connectTimer = 0.0f;
	}

	thisClient->SendPacket(newPacket);
}

GameObject* NetworkedGame::SpawnNetworkedPlayer(int playerID) {
	// 计算生成位置，排排站
	Vector3 pos = Vector3(-60 + (playerID * 10), 5, 60);
	Player* newPlayer = AddPlayerToWorld(pos, 1.0f);
	// 设置 NetworkObject, NetworkID = playerID
	newPlayer->SetNetworkObject(new NetworkObject(*newPlayer, playerID));
	// 存入 MyGame 的 vector
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
	float startX = 80.0f; // 屏幕右侧
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
		delete thisClient;
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
	rival->SetNetworkObject(new NetworkObject(*rival, 5));
	goose->SetNetworkObject(new NetworkObject(*goose, 6));
	patrolEnemy->SetNetworkObject(new NetworkObject(*patrolEnemy, 7));
	packageObject->SetNetworkObject(new NetworkObject(*packageObject, 8));;

	int idCounter = 10;
	// allocate numbers to coins
	for (auto& coin : coins) {
		if (!coin->GetNetworkObject()) {
			coin->SetNetworkObject(new NetworkObject(*coin, idCounter++));
		}
	}

	// allocate to stones and cube stones
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