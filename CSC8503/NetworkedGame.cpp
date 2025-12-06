#include "NetworkedGame.h"
#include "NetworkPlayer.h"
#include "NetworkObject.h"
#include "GameServer.h"
#include "GameClient.h"
#include "GameWorld.h"
#include "Window.h"

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
	localPlayer = nullptr;
	NetworkBase::Initialise();
	timeToNextPacket = 0.0f;
	packetsToSnapshot = 0;
}

NetworkedGame::~NetworkedGame() {
	delete thisServer;
	delete thisClient;
}

void NetworkedGame::StartAsServer() {
	thisServer = new GameServer(NetworkBase::GetDefaultPort(), 4);
	// 注册处理: 客户端发来的输入包 + 确认包
	thisServer->RegisterPacketHandler(BasicNetworkMessages::Client_Update, this);
	thisServer->RegisterPacketHandler(BasicNetworkMessages::Received_State, this);

	InitWorld(); // 初始化游戏场景
	std::cout << "Server Started." << std::endl;
}

void NetworkedGame::StartAsClient(char a, char b, char c, char d) {
	thisClient = new GameClient();
	if (thisClient->Connect(a, b, c, d, NetworkBase::GetDefaultPort())) {
		std::cout << "Connected to Server!" << std::endl;
	}

	// 注册处理: 服务器发来的 增量状态、全量状态、玩家连接/断开
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Delta_State, this);
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Full_State, this);
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Player_Connected, this);
	thisClient->RegisterPacketHandler(BasicNetworkMessages::Player_Disconnected, this);

	// 客户端不需要初始化完整的Level逻辑，只需等待服务器发送物体数据
	// 但为了地形等静态物体，通常也需要加载基础场景
	InitWorld();

	// 客户端清除所有动态物体，等待服务器同步
	// world.ClearAndErase(); // 视具体需求决定是否清除
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
		timeToNextPacket += 1.0f / 20.0f; //20hz server/client update
	}

	if (!thisServer && Window::GetKeyboard()->KeyPressed(KeyCodes::F9)) {
		StartAsServer();
	}
	if (!thisClient && Window::GetKeyboard()->KeyPressed(KeyCodes::F10)) {
		StartAsClient(127, 0, 0, 1);
	}

	// 只有服务器需要运行完整的物理和逻辑更新
	// 客户端只需要运行渲染更新 (和本地插值)
	if (thisServer) {
		MyGame::UpdateGame(dt); // 服务器运行物理
	}
	else {
		// 客户端只更新摄像机和渲染，不运行物理模拟
		world.UpdateWorld(dt);
		//renderer.Update(dt);
		physics.Update(dt); // 客户端通常仍然运行物理update以处理插值，但要关闭重力等
	}
}

// Server Logic
void NetworkedGame::UpdateAsServer(float dt) {
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
	// 服务器处理接收到的包
	// === Server Side Handling ===
	if (thisServer) {
		switch (type) {
		case BasicNetworkMessages::Client_Update: {
			// 收到客户端输入，更新对应玩家的受力/状态
			ClientPacket* packet = (ClientPacket*)payload;
			ServerProcessClientInput(source, packet);
			break;
		}
		case BasicNetworkMessages::Player_Connected: {
			// ENet 握手层已处理，但如果需要应用层逻辑可在此处
			std::cout << "Server: Client " << source << " connected event." << std::endl;
			break;
		}
		}
	}
	// === Client Side Handling ===
	else if (thisClient) {
		switch (type) {
		case BasicNetworkMessages::Full_State:
		case BasicNetworkMessages::Delta_State: {
			// 查找对应的 NetworkObject 并更新
			int objectID = -1;
			if (type == Full_State) objectID = ((FullPacket*)payload)->objectID;
			else objectID = ((DeltaPacket*)payload)->objectID;

			// 在世界中找到该 NetworkObject
			// 这里需要一个从 networkID 查找 GameObject 的机制
			// 简单遍历演示：
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
		case BasicNetworkMessages::Player_Connected: {
			// 收到服务器通知：有新玩家（或自己）加入
			NewPlayerPacket* pkt = (NewPlayerPacket*)payload;
			SpawnPlayer(pkt->playerID);
			break;
		}
		case BasicNetworkMessages::Player_Disconnected: {
			// 移除玩家逻辑...
			break;
		}
		}
	}
}

void NetworkedGame::ServerProcessClientInput(int playerID, ClientPacket* packet) {
	// 1. 查找玩家对象。如果没有，则生成。
	if (serverPlayers.find(playerID) == serverPlayers.end()) {
		std::cout << "Server: New Player Spawned for Client " << playerID << std::endl;
		GameObject* newPlayer = SpawnPlayer(playerID);
		serverPlayers[playerID] = newPlayer;

		// 通知所有客户端有新玩家加入 (这一步实际应该遍历所有玩家发给新客户端，并把新玩家发给所有旧客户端)
		NewPlayerPacket newPlayerPkt(playerID, newPlayer->GetTransform().GetPosition());
		thisServer->SendGlobalPacket(newPlayerPkt);
	}

	GameObject* playerObj = serverPlayers[playerID];
	if (!playerObj) return;

	// 2. 将输入转换为物理力 (服务器权威运算)
	PhysicsObject* phys = playerObj->GetPhysicsObject();
	if (!phys) return;

	// 解析包中的数据
	float speed = 20.0f; // 移动速度
	float rotationSpeed = 5.0f;

	// 解析轴向输入
	Vector3 inputDir = Vector3(0, 0, 0);
	// 假设 packet->axis 存的是 -100 到 100 的值
	if (packet->axis[0] != 0) inputDir.z = (float)packet->axis[0] / 100.0f; // W/S
	if (packet->axis[1] != 0) inputDir.x = (float)packet->axis[1] / 100.0f; // A/D

	// 解析 Yaw (视角)
	float yaw = packet->yaw;
	Quaternion cameraRot = Quaternion::EulerAnglesToQuaternion(0, yaw, 0);

	// 只有当有输入时才移动
	if (Vector::LengthSquared(inputDir) > 0) {
		Vector3 targetDir = cameraRot * inputDir;
		targetDir = Vector::Normalise(targetDir);

		// 服务器直接应用力
		phys->AddForce(targetDir * speed);

		// 设置朝向
		// 注意：这里需要平滑插值逻辑，简化为直接设置
		// 实际项目中应复用 Player::PlayerControl 中的朝向计算逻辑
		playerObj->GetTransform().SetOrientation(cameraRot);
	}

	// 处理跳跃
	if (packet->buttonstates[0] > 0) { // 假设 0 号位是空格
		// 需要服务器端的 IsGrounded 检查
		phys->ApplyLinearImpulse(Vector3(0, 10, 0));
	}
}

// Client Logic
void NetworkedGame::UpdateAsClient(float dt) {
	// 1. 收集本地输入
	ClientPacket newPacket;

	// 收集键盘 WASD
	int forward = 0;
	int right = 0;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::W)) forward -= 100;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::S)) forward += 100;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::A)) right -= 100;
	if (Window::GetKeyboard()->KeyDown(KeyCodes::D)) right += 100;

	newPacket.axis[0] = forward;
	newPacket.axis[1] = right;

	// 收集按键 (空格跳跃)
	if (Window::GetKeyboard()->KeyDown(KeyCodes::SPACE)) {
		newPacket.buttonstates[0] = 1;
	}
	if (Window::GetMouse()->ButtonPressed(MouseButtons::Left)) {
		newPacket.buttonstates[1] = 1;
	}

	// 收集视角 (Yaw)
	newPacket.yaw = world.GetMainCamera().GetYaw();

	newPacket.lastID = 0; // 可以用于丢包检测机制

	// 2. 发送给服务器
	thisClient->SendPacket(newPacket);

	// 3. 客户端不需要在这里更新物理位置，位置由 ReceivePacket 中的状态包驱动
}

GameObject* NetworkedGame::SpawnPlayer(int playerID, bool isSelf) {
	// 为了简化，生成位置可以随机或者指定
	Vector3 pos = Vector3(0, 5, 0);
	Player* newPlayer = AddPlayerToWorld(pos, 1.0f); // 复用 MyGame 的生成逻辑

	// 必须给这个对象分配 NetworkObject 组件，否则无法同步
	// 注意：NetworkID 必须服务器和客户端一致。这里暂时用 playerID 作为 networkID (简单处理)
	// 实际项目中需要专门的 NetworkID 生成器
	newPlayer->SetNetworkObject(new NetworkObject(*newPlayer, playerID));

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

// Not used now - but could be used to clean up old states
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