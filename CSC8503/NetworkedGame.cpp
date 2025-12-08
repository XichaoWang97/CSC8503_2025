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

using namespace NCL;
using namespace CSC8503;

struct GlobalStatePacket : public GamePacket {
	int rivalScore;
	int networkScore; // score of player
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

NetworkedGame::NetworkedGame(GameWorld& gameWorld, GameTechRendererInterface& renderer, PhysicsSystem& physics) : MyGame(gameWorld, renderer, physics)
{
	thisServer = nullptr;
	thisClient = nullptr;
	NetworkBase::Initialise();
	timeToNextPacket = 0.0f;
	packetsToSnapshot = 0;
}

NetworkedGame::~NetworkedGame() {
	delete thisServer;
	delete thisClient;
}

void NetworkedGame::StartAsServer(int playerCount) {
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
	Disconnect();
	thisClient = new GameClient();
	if (thisClient->Connect(a, b, c, d, NetworkBase::GetDefaultPort())) {
		std::cout << "Connected to Server!" << std::endl;
	}

	thisClient->RegisterPacketHandler(BasicNetworkMessages::Delta_State, this);
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Full_State, this);
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Player_Connected, this);
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Player_Disconnected, this);
	thisClient->RegisterPacketHandler(GLOBAL_STATE_MSG, this);

	InitWorld();
	InitNetworkObjectToWorld();

	// 【核心逻辑】客户端也初始化容器, 根据数字初始化 Vector。这里为了测试，假设我们也是 2 人房
	int estimatedPlayers = 2;

	for (int i = 0; i < estimatedPlayers; ++i) {
		GameObject* p = SpawnNetworkedPlayer(i);

		// 客户端必须把所有网络对象设为无物理，防止抖动
		if (p->GetPhysicsObject()) {
			p->GetPhysicsObject()->SetInverseMass(0.0f);
		}
	}
	// 假设我是 1 号位 (第一个连入的客户端)
	localPlayerID = 1;
	std::cout << "Client Started. Assuming I am Player ID " << localPlayerID << std::endl;


	// 处理 Goose/AI 的本地物理禁用
	if (goose && goose->GetPhysicsObject()) goose->GetPhysicsObject()->SetInverseMass(0.0f);
	if (rival && rival->GetPhysicsObject()) rival->GetPhysicsObject()->SetInverseMass(0.0f);

	// 【新增修复代码】告诉客户端的 AI 玩家列表在哪里（虽然它们主要是视觉表现）
	if (goose) goose->SetPlayerList(&players);
	if (rival) rival->SetPlayerList(&players);
	if (patrolEnemy) patrolEnemy->SetPlayerList(&players);
}

void NetworkedGame::UpdateGame(float dt) {
	// 1. 客户端逻辑：每帧都运行，确保不漏掉按键
	if (thisClient) {
		UpdateAsClient(dt);
	}

	// 2. 服务器逻辑：依然保持 20Hz (因为这是发送大量快照，太频繁会卡死)
	timeToNextPacket -= dt;
	if (timeToNextPacket <= 0.0f) {
		if (thisServer) {
			UpdateAsServer(dt);
		}
		timeToNextPacket += 1.0f / 20.0f;
	}

	// 3. 网络更新
	if (thisServer) thisServer->UpdateServer();
	if (thisClient) thisClient->UpdateClient();

	MyGame::UpdateGame(dt);
	DrawNetworkHUD();
}

// Server Logic
void NetworkedGame::UpdateAsServer(float dt) {
	// 1. 服务器独占的重力控制逻辑
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::G)) {
		useGravity = !useGravity;
		physics.UseGravity(useGravity);
	}

	bool p1IsDead = false;
	bool p2IsDead = false;
	bool p2IsConnected = isP2ConnectedServer;

	if (goose) {
		Vector3 goosePos = goose->GetTransform().GetPosition();

		// 检查 P1 (Server)
		if (!players.empty() && players[0]) {
			if (Vector::Length(players[0]->GetTransform().GetPosition() - goosePos) < 4.0f) {
				players[0]->SetDead(true);
			}
			p1IsDead = players[0]->IsDead();
		}

		// 检查 P2 (Client)
		if (players.size() > 1 && players[1] && p2IsConnected) {
			if (Vector::Length(players[1]->GetTransform().GetPosition() - goosePos) < 4.0f) {
				players[1]->SetDead(true);
			}
			p2IsDead = players[1]->IsDead();
		}
	}

	// 判断游戏失败条件
	// 1. 所有玩家都死了
	bool allPlayersDead = p1IsDead && (p2IsConnected ? p2IsDead : true); // 如果P2没连，P1死即全死

	// 2. Rival 赢了 (分数达标 且 在终点) - 这部分逻辑保留你 MyGame 原有的判定，这里只做同步
	// 为了简化，这里直接检查 isGameOver 标记（需要确保 MyGame 里 Rival 胜利会设 isGameOver）
	if (rival && rival->GetScore() >= winningScore) {
		// 这里做一个简单判定，假设 Rival 总是能进终点
		gameOverReason = GameOverReason::RivalWin;
		isGameOver = true;
	}
	else if (allPlayersDead) {
		gameOverReason = GameOverReason::GooseCatch;
		isGameOver = true;
	}
	else {
		gameOverReason = GameOverReason::None;
		isGameOver = false;
	}

	// 2. 组装全局状态包 (每一帧或每隔几帧发送一次，看你需求)
	// 这里为了简单，我们把它放在 Snapshot 的逻辑里一起发，或者单独发
	GlobalStatePacket statePacket;
	statePacket.useGravity = useGravity;

	// 同步分数 (解决 Rival 得分不对的问题)
	if (rival) statePacket.rivalScore = rival->GetScore();
	// 假设我们只同步 package 的收集数作为分数
	if (packageObject) statePacket.networkScore = packageObject->GetCollectionCount();
	// [新增] 填入状态
	statePacket.p2Connected = p2IsConnected;
	statePacket.p1Dead = p1IsDead;
	statePacket.p2Dead = p2IsDead;
	statePacket.gameOverReason = (int)gameOverReason;

	thisServer->SendGlobalPacket(statePacket);

	// send snap shot
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
		switch (type) {
		case BasicNetworkMessages::Client_Update: {
			// Server gets input of Client -> control relative Player
			int playerIndex = source + 1;
			
			ServerProcessClientInput(playerIndex, (ClientPacket*)payload);
			break;
		}
		case BasicNetworkMessages::Player_Connected: { // p2 connected
			isP2ConnectedServer = true;
			break;
		}
		}
	}
	else if (thisClient) {
		switch (type) {
		case BasicNetworkMessages::Full_State:
		case BasicNetworkMessages::Delta_State: {
			int objectID = -1;
			if (type == Full_State) objectID = ((FullPacket*)payload)->objectID;
			else objectID = ((DeltaPacket*)payload)->objectID;

			// 遍历世界找到 ID 匹配的物体更新位置
			std::vector<GameObject*>::const_iterator first;
			std::vector<GameObject*>::const_iterator last;
			world.GetObjectIterators(first, last);
			for (auto i = first; i != last; ++i) {
				NetworkObject* o = (*i)->GetNetworkObject();
				if (o && o->GetNetworkID() == objectID) {
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
		this->score = packet->networkScore;

		// [新增] 同步游戏结束原因
		this->gameOverReason = (GameOverReason)packet->gameOverReason;
		if (this->gameOverReason != GameOverReason::None) {
			this->isGameOver = true;
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

	// 这里的逻辑必须和 Server 解析的逻辑对应
	if (Window::GetKeyboard()->KeyDown(KeyCodes::W)) forward -= 100; // Z-
	if (Window::GetKeyboard()->KeyDown(KeyCodes::S)) forward += 100; // Z+
	if (Window::GetKeyboard()->KeyDown(KeyCodes::A)) right -= 100;   // X-
	if (Window::GetKeyboard()->KeyDown(KeyCodes::D)) right += 100;   // X+

	newPacket.axis[0] = forward;
	newPacket.axis[1] = right;

	// 发送 Yaw，这样服务器才能知道你面朝哪里
	newPacket.yaw = world.GetMainCamera().GetYaw();

	newPacket.lastID = 0;

	// 按键状态
	newPacket.buttonstates[0] = 0;
	newPacket.buttonstates[1] = 0;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::SPACE)) newPacket.buttonstates[0] = 1;
	if (Window::GetMouse()->ButtonPressed(MouseButtons::Left)) newPacket.buttonstates[1] = 1;
	// ================= [新增/修改部分 START] =================

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

			// 3. 关键点：处理 IgnoreInput
			// 在 StartAsClient 中，为了防止客户端物理和服务器打架，通常设置了 SetIgnoreInput(true)。
			// 如果 Player::Update 里面第一行就是 if(ignoreInput) return; 那么即使设置了 Input 也没用。
			// 解决方法是：临时允许处理 Input，或者确保你的 Player 逻辑分开了移动和交互。

			bool wasIgnoring = localPlayer->GetIgnoreInput(); // 假设你有这个 getter，或者直接手动控制

			// 强制允许输入处理一帧 (为了触发射线视觉效果)
			// 注意：这可能会导致客户端有一瞬间的物理预测抖动，但对于射线交互通常是可以接受的
			localPlayer->SetIgnoreInput(false);

			localPlayer->SetPlayerInput(inputs);

			// 如果你的 Player::Update 是在 NetworkedGame::UpdateGame 里统一调用的，
			// 这里设置完 Input 后，等到 Update 循环到它时就会自动画线了。

			// 如果不想干扰物理移动，可以在 Update 之后把 Ignore 设回去，
			// 但通常简单的做法是保持 False，但把 InverseMass 设为 0 (你代码里已经做了)，这样就不会被物理推走。
		}
	}
	// ================= [新增/修改部分 END] =================
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

void NetworkedGame::UpdateKeys() {
	if (thisServer) {
		if (Window::GetKeyboard()->KeyPressed(KeyCodes::G)) {
			useGravity = !useGravity;
			physics.UseGravity(useGravity);

			// inform others
			std::cout << "Server toggled gravity!" << std::endl;

			GlobalStatePacket statePacket;
			statePacket.useGravity = useGravity;
			statePacket.rivalScore = rival ? rival->GetScore() : 0;
			statePacket.networkScore = packageObject ? packageObject->GetCollectionCount() : 0;

			thisServer->SendGlobalPacket(statePacket);
		}
	}
	else if (thisClient) {
		// 【关键】客户端这里留空！
		// 意味着：客户端按 G 键没有任何反应。
		// 只有等到 Server 发来 GlobalStatePacket，ReceivePacket 函数被触发时，重力才会变。
	}
	// 情况C：既不是Client也不是Server（防御性代码，理论上不会发生）
	else {
		MyGame::UpdateKeys();
	}
}

// Draw NetworkedGame UI
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

// NetworkedGame.cpp
void NetworkedGame::ResetGame() {
	InitWorld();
	if (thisServer) {
		StartAsServer(2);
	}
}