#include "NetworkedGame.h"
#include "NetworkPlayer.h"
#include "NetworkObject.h"
#include "GameServer.h"
#include "GameClient.h"
#include "GameWorld.h"
#include "Window.h"
#include "Matrix.h"
#define COLLISION_MSG 30

using namespace NCL;
using namespace CSC8503;

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
	thisServer = new GameServer(NetworkBase::GetDefaultPort(), 3);
	// 注册处理: 客户端发来的输入包 + 确认包
	thisServer->RegisterPacketHandler(BasicNetworkMessages::Client_Update, this);
	thisServer->RegisterPacketHandler(BasicNetworkMessages::Received_State, this);
	InitWorld();

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
	thisClient = new GameClient();
	if (thisClient->Connect(a, b, c, d, NetworkBase::GetDefaultPort())) {
		std::cout << "Connected to Server!" << std::endl;
	}

	thisClient->RegisterPacketHandler(BasicNetworkMessages::Delta_State, this);
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Full_State, this);
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Player_Connected, this);
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Player_Disconnected, this);

	InitWorld();

	// 【核心逻辑】客户端也初始化容器
	// 按照你的要求：根据数字初始化 Vector。这里为了测试，假设我们也是 2 人房
	// 实际应用中，应该由服务器发包告知 "TotalPlayers"
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
}

// Server Logic
void NetworkedGame::UpdateAsServer(float dt) {
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
		std::cout << "Server_!!!!!!!!!!!" << std::endl;
		switch (type) {
		case BasicNetworkMessages::Client_Update: {
			// Server 收到 Client 的输入 -> 控制对应的 Player
			// 注意：Source 是 Client ID。
			// 在我们的简单模型里，Client 1 控制 Player 1。
			// ENet 的 peerID 通常从 0 开始，这可能需要映射一下。
			// 假设 Client 连接进来分配的 ID 正好对应我们的 Player ID 1...
			// 实际上 source 可能是随机的大数，这里简单假设 source 就是 player index
			// 如果有问题，可以用 map<int source, int playerIndex> 来映射
			int playerIndex = source + 1;
			// 暂时假定 source 1 就是 Player 1
			ServerProcessClientInput(playerIndex, (ClientPacket*)payload);
			break;
		}
		}
	}
	else if (thisClient) {
		std::cout << "Client_!!!!!!!!!!!" << std::endl;
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
	if (Window::GetKeyboard()->KeyPressed(KeyCodes::SPACE)) newPacket.buttonstates[0] = 1;
	if (Window::GetMouse()->ButtonPressed(MouseButtons::Left)) newPacket.buttonstates[1] = 1;

	thisClient->SendPacket(newPacket);

	// ---------------------- 【新增】同步画面逻辑 ----------------------
	// 遍历所有物体，如果它有 NetworkObject，就把它“瞬移”到服务器发来的最新位置

	// 获取当前客户端的 NetworkObject 列表
	/*std::vector<GameObject*>::const_iterator first;
	std::vector<GameObject*>::const_iterator last;
	world.GetObjectIterators(first, last);

	for (auto i = first; i != last; ++i) {
		NetworkObject* o = (*i)->GetNetworkObject();
		if (!o) continue;

		// 这里需要用到插值(Interpolation)或者直接覆盖(Snap)。
		// 为了先解决“不动”的问题，我们先用最简单的“直接覆盖 (Snap to latest)”。

		// 注意：你需要确保 NetworkObject 类中有 GetLatestNetworkState() 方法
		// 如果你的 NCL 框架中 NetworkObject 没有这个方法，通常它是通过 stateHistory 获取的
		// 下面是一个通用的 NCL 框架获取最新状态的写法：

		Transform& transform = (*i)->GetTransform();

		// 尝试获取最新的网络状态并应用
		// 假设 NetworkObject 内部维护了一个 stateHistory 或者 lastFullState
		// 因为我看不到 NetworkObject.h，通常做法如下：

		NetworkState lastState;
		if (o->GetLatestNetworkState(lastState)) { // 这一步从 NetworkObject 取出最新收到的包
			transform.SetPosition(lastState.position);
			transform.SetOrientation(lastState.orientation);
		}
	}*/
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
