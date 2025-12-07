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
	thisServer = new GameServer(NetworkBase::GetDefaultPort(), 4);
	// 注册处理: 客户端发来的输入包 + 确认包
	thisServer->RegisterPacketHandler(BasicNetworkMessages::Client_Update, this);
	thisServer->RegisterPacketHandler(BasicNetworkMessages::Received_State, this);

	InitWorld(); // 初始化游戏场景
	// 【核心逻辑】根据人数生成 Player 容器
	// Player 0 = Server (Me)
	// Player 1...N = Clients
	for (int i = 0; i < playerCount; ++i) {
		GameObject* newPlayer = SpawnNetworkedPlayer(i);

		// 记录到 Server 的 Map 中，方便通过 NetworkID 查找
		serverPlayers[i] = newPlayer;
	}

	localPlayerID = 0; // 服务器控制 0 号
	std::cout << "Server Started with " << playerCount << " players container." << std::endl;
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
}

void NetworkedGame::UpdateGame(float dt) {
	timeToNextPacket -= dt;
	if (timeToNextPacket < 0) {
		if (thisServer) {
			UpdateAsServer(dt);
		}
		else if (thisClient) {
			UpdateAsClient(dt);
		}
		timeToNextPacket += 1.0f / 20.0f;
	}

	if (thisServer) thisServer->UpdateServer();
	if (thisClient) thisClient->UpdateClient();

	// F9 启动 2 人 Server
	if (!thisServer && !thisClient && Window::GetKeyboard()->KeyPressed(KeyCodes::F9)) {
		StartAsServer(2);
	}
	// F10 启动 Client
	if (!thisServer && !thisClient && Window::GetKeyboard()->KeyPressed(KeyCodes::F10)) {
		StartAsClient(127, 0, 0, 1);
	}

	MyGame::UpdateGame(dt);
}

// Server Logic
void NetworkedGame::UpdateAsServer(float dt) {
	// 1. 处理 Server 本地玩家 (Player 0) 的输入
	if (localPlayerID < players.size()) {
		Player* serverPlayer = players[localPlayerID];
		// 简单的本地控制逻辑
		// 获取摄像机的 Yaw 角度
		float yaw = world.GetMainCamera().GetYaw();

		// === 【修改开始】 手动计算 Forward 和 Right ===

		// 创建一个基于 Yaw 的旋转矩阵 (绕 Y 轴旋转)
		// 这样计算出来的方向本身就是水平的，不需要再手动设 y=0
		Matrix3 yawRotation = Matrix::RotationMatrix3x3(yaw, Vector3(0, 1, 0));
		// NCL 坐标系中，默认 Forward 是 -Z (0, 0, -1)，Right 是 +X (1, 0, 0)
		// 使用矩阵旋转这些默认向量
		Vector3 fwd = yawRotation * Vector3(0, 0, -1);
		Vector3 right = yawRotation * Vector3(1, 0, 0);

		float speed = 30.0f;
		PhysicsObject* phys = serverPlayer->GetPhysicsObject();

		if (Window::GetKeyboard()->KeyDown(KeyCodes::W)) phys->AddForce(fwd * speed);
		if (Window::GetKeyboard()->KeyDown(KeyCodes::S)) phys->AddForce(-fwd * speed);
		if (Window::GetKeyboard()->KeyDown(KeyCodes::A)) phys->AddForce(-right * speed);
		if (Window::GetKeyboard()->KeyDown(KeyCodes::D)) phys->AddForce(right * speed);
	}

	// 2. 发送快照
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
		if (!o) {
			continue;
		}
		//TODO - you'll need some way of determining
		//when a player has sent the server an acknowledgement
		//and store the lastID somewhere. A map between player
		//and an int could work, or it could be part of a 
		//NetworkPlayer struct. 
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
			// Server 收到 Client 的输入 -> 控制对应的 Player
			// 注意：Source 是 Client ID。
			// 在我们的简单模型里，Client 1 控制 Player 1。
			// ENet 的 peerID 通常从 0 开始，这可能需要映射一下。
			// 假设 Client 连接进来分配的 ID 正好对应我们的 Player ID 1...
			// 实际上 source 可能是随机的大数，这里简单假设 source 就是 player index
			// 如果有问题，可以用 map<int source, int playerIndex> 来映射

			// 暂时假定 source 1 就是 Player 1
			ServerProcessClientInput(source, (ClientPacket*)payload);
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
}

void NetworkedGame::ServerProcessClientInput(int playerID, ClientPacket* packet) {
	// 查找对应的玩家对象
	GameObject* playerObj = nullptr;

	// 从 vector 查找更直接
	if (playerID >= 0 && playerID < players.size()) {
		playerObj = players[playerID];
	}

	if (!playerObj) return;

	PhysicsObject* phys = playerObj->GetPhysicsObject();
	if (!phys) return;

	float speed = 50.0f; // 力度大一点
	Vector3 inputDir = Vector3(0, 0, 0);
	if (packet->axis[0] != 0) inputDir.z = (float)packet->axis[0] / 100.0f;
	if (packet->axis[1] != 0) inputDir.x = (float)packet->axis[1] / 100.0f;

	// 使用 Client 传来的 Yaw 确定方向
	Quaternion cameraRot = Quaternion::EulerAnglesToQuaternion(0, packet->yaw, 0);
	Vector3 worldDir = cameraRot * inputDir;
	worldDir.y = 0;
	worldDir = Vector::Normalise(worldDir);

	if (Vector::LengthSquared(worldDir) > 0) {
		phys->AddForce(worldDir * speed);
	}

	if (packet->buttonstates[0] > 0) {
		phys->ApplyLinearImpulse(Vector3(0, 10, 0));
	}
}

// Client Logic
void NetworkedGame::UpdateAsClient(float dt) {
	// 收集键盘输入，发给服务器去控制 "My Player"
	ClientPacket newPacket;

	int forward = 0;
	int right = 0;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::W)) forward -= 100;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::S)) forward += 100;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::A)) right -= 100;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::D)) right += 100;

	newPacket.axis[0] = forward;
	newPacket.axis[1] = right;
	newPacket.yaw = world.GetMainCamera().GetYaw();
	newPacket.lastID = 0;

	if (Window::GetKeyboard()->KeyPressed(KeyCodes::SPACE)) newPacket.buttonstates[0] = 1;

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
