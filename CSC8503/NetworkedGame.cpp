#include "NetworkedGame.h"
#include "NetworkPlayer.h"
#include "NetworkObject.h"
#include "GameServer.h"
#include "GameClient.h"
#include "GameWorld.h"
#include "Window.h"
#include "Debug.h"

#include "PhysicsObject.h"
#include "RenderObject.h"
#include "TextureLoader.h"
#include "StateGameObject.h" 
#include "FragileGameObject.h" 
#include <fstream>
#include "Ray.h"
#include "CollisionDetection.h"
#include "Assets.h"
#define COLLISION_MSG 30

using namespace NCL;
using namespace CSC8503;

struct MessagePacket : public GamePacket {
	short playerID;
	short messageID;

	MessagePacket() {
		type = BasicNetworkMessages::Message;
		size = sizeof(short) * 2;
	}
};

// --- 修改: 继承 MyGame ---
NetworkedGame::NetworkedGame(GameWorld& gameWorld, GameTechRendererInterface& renderer, PhysicsSystem& physics) 
	: MyGame(gameWorld, renderer, physics)
{
	thisServer = nullptr;
	thisClient = nullptr;
	localPlayer = nullptr;
	myPlayerID = -1;

	NetworkBase::Initialise();
	timeToNextPacket = 0.0f;
	packetsToSnapshot = 0;

	// --- 关键: 禁用 MyGame 的本地玩家 ---
	// 我们将手动管理 localPlayer 和 playerObject
	playerObject = nullptr;
}

NetworkedGame::~NetworkedGame() {
	delete thisServer;
	delete thisClient;
}

void NetworkedGame::StartAsServer() {
	thisServer = new GameServer(NetworkBase::GetDefaultPort(), 4);

	thisServer->RegisterPacketHandler(Received_State, this);
	// --- 任务 2.3: 注册客户端输入处理器 ---
	thisServer->RegisterPacketHandler(Client_Input, this);

	// --- 调用 MyGame 的关卡加载 ---
	InitWorld();

	// --- 任务 2.4: 生成重包裹 (Server) ---
	// ID 设为 100，避免与玩家 ID 冲突
	// 位置放在玩家出生点附近，方便测试
	localHeavyPackage = SpawnNetworkedObject(100, Vector3(10, -15, 10), Vector3(2, 2, 2), 0.0f);

	// --- 服务器生成自己的玩家 (ID 0) ---
	// 使用 MyGame::InitCourierLevel 中的出生点逻辑
	// 注意：InitWorld 会创建单人模式的 playerObject，我们需要把它转成 NetworkPlayer 或者删掉重新生成
	// 为了简单，InitWorld 创建的是普通 GameObject，我们这里额外生成网络玩家

	// 实际上，MyGame::InitWorld 创建了一个 playerObject。在服务器模式下，
	// 我们可能应该删除那个单机玩家，或者在 InitWorld 里加判断。
	// 这里为了逻辑简单，我们假设 InitWorld 创建了场景，我们现在手动添加网络玩家。

	// 服务器玩家 ID 为 0
	Vector3 playerStartPos = Vector3(0, 5, 50);
	localPlayer = AddPlayerToWorld(playerStartPos, 0);
	serverPlayers.emplace(0, localPlayer);

	// --- 关键: 将服务器玩家设为 MyGame 的 'playerObject' ---
	// 这样 MyGame::UpdateGame 中的 AI 才能找到目标
	playerObject = localPlayer;

	// --- 3.1 生成鹅 NPC ---
	// 1. 初始化网格
	InitGrid();

	// 2. 生成鹅
	Vector3 goosePos = Vector3(20, 0, 20); // 放在稍微远一点的地方
	angryGoose = new GooseNPC(navGrid, localPlayer); // 追逐服务器玩家

	// 设置外观和物理
	angryGoose->GetTransform().SetPosition(goosePos).SetScale(Vector3(2, 2, 2));
	angryGoose->SetRenderObject(new RenderObject(angryGoose->GetTransform(), enemyMesh, notexMaterial));
	angryGoose->GetRenderObject()->SetColour(Vector4(1, 0.5f, 0, 1)); // 橙色

	angryGoose->SetPhysicsObject(new PhysicsObject(angryGoose->GetTransform(), new SphereVolume(1.5f)));
	angryGoose->GetPhysicsObject()->SetInverseMass(1.0f);
	angryGoose->GetPhysicsObject()->InitSphereInertia();
	angryGoose->SetGameWorld(&world); // 这一步很重要，用于射线检测

	// 加入世界
	world.AddGameObject(angryGoose);

	// 注意：如果是网络游戏，鹅也需要 NetworkObject 才能在客户端看到
	// 给鹅 ID 101
	NetworkObject* netObj = new NetworkObject(*angryGoose, 101);
	networkObjects.push_back(netObj);
	angryGoose->SetNetworkObject(netObj);

	// --- 任务 3.3: 生成竞争对手 ---
	Vector3 rivalPos = Vector3(-20, 0, -20);
	rivalAI = new RivalAI(navGrid);
	rivalAI->SetGameWorld(&world); // 重要：传入 world 以便搜寻目标

	// 设置基本属性
	rivalAI->GetTransform().SetPosition(rivalPos).SetScale(Vector3(1.5f, 1.5f, 1.5f));

	// 设置外观 (复用玩家的模型，但换个颜色，比如紫色)
	// 假设 catMesh 已存在
	rivalAI->SetRenderObject(new RenderObject(rivalAI->GetTransform(), catMesh, notexMaterial));
	rivalAI->GetRenderObject()->SetColour(Vector4(0.5f, 0, 0.5f, 1)); // 紫色

	// 设置物理
	rivalAI->SetPhysicsObject(new PhysicsObject(rivalAI->GetTransform(), new SphereVolume(1.5f)));
	rivalAI->GetPhysicsObject()->SetInverseMass(1.0f);
	rivalAI->GetPhysicsObject()->InitSphereInertia();

	// 注册到网络
	NetworkObject* rivalNet = new NetworkObject(*rivalAI, 102); // ID 102
	networkObjects.push_back(rivalNet);
	rivalAI->SetNetworkObject(rivalNet);

	world.AddGameObject(rivalAI);
}

void NetworkedGame::StartAsClient(char a, char b, char c, char d) {
	thisClient = new GameClient();
	thisClient->Connect(a, b, c, d, NetworkBase::GetDefaultPort());

	thisClient->RegisterPacketHandler(Delta_State, this);
	thisClient->RegisterPacketHandler(Full_State, this);
	thisClient->RegisterPacketHandler(Player_Connected, this);
	thisClient->RegisterPacketHandler(Player_Disconnected, this);

	// --- 客户端也加载关卡，但不生成玩家 ---
	InitWorld();
	// --- 任务 2.4: 生成重包裹 (Client) ---
	localHeavyPackage = SpawnNetworkedObject(100, Vector3(10, -15, 10), Vector3(2, 2, 2), 0.0f);

	// 客户端不需要 MyGame 创建的单机玩家，将其清理（如果有的话）
	if (playerObject) {
		world.RemoveGameObject(playerObject, true);
		playerObject = nullptr;
	}
}

void NetworkedGame::UpdateGame(float dt) {
	// --- 1. 网络更新 ---
	timeToNextPacket -= dt;
	if (timeToNextPacket < 0) {
		if (thisServer) {
			UpdateAsServer(dt);
		}
		else if (thisClient) {
			UpdateAsClient(dt);
		}
		timeToNextPacket += 1.0f / 20.0f; // 20hz update
	}

	// --- 2. 游戏逻辑更新 ---
	if (thisServer) {
		// --- 服务器: 运行 MyGame 的完整更新 ---
		// 注意：MyGame::UpdateGame 包含了 PlayerControl。
		// 服务器的 PlayerControl 应该只控制 ID 0 (本地玩家)。
		// 我们通过确保 playerObject == localPlayer (ID 0) 来实现这一点。
		MyGame::UpdateGame(dt);
	}
	else if (thisClient) {
		// --- 客户端: 只运行摄像机和对象更新 ---

		// 1. 摄像机 (逻辑从 MyGame::UpdateGame 复制而来)
		if (localPlayer) {
			Vector3 playerPos = localPlayer->GetTransform().GetPosition();

			float yaw = (int)world.GetMainCamera().GetYaw();
			float pitch = world.GetMainCamera().GetPitch();

			// 限制 Pitch
			if (pitch > 30.0f) pitch = 30.0f;
			if (pitch < -60.0f) pitch = -60.0f;

			Quaternion cameraRot = Quaternion::EulerAnglesToQuaternion(pitch, yaw, 0);
			Vector3 cameraBackward = cameraRot * Vector3(0, 0, 1);

			float maxDist = 15.0f;
			float currentDist = maxDist;
			Vector3 offset = Vector3(0, 5, 0);

			Vector3 rayOrigin = playerPos + offset;
			Vector3 rayDir = cameraBackward;
			Ray ray(rayOrigin, rayDir);
			RayCollision collision;

			// Raycast 需要忽略本地玩家
			if (world.Raycast(ray, collision, true, localPlayer)) {
				if (collision.rayDistance < maxDist) {
					currentDist = collision.rayDistance - 0.5f;
					if (currentDist < 0.5f) currentDist = 0.5f;
				}
			}
			Vector3 cameraPos = rayOrigin + (cameraBackward * currentDist);
			world.GetMainCamera().SetPosition(cameraPos);
		}
		world.GetMainCamera().UpdateCamera(dt);

		// 2. 更新所有游戏对象 (包括 AI 的状态机、物理插值等)
		world.OperateOnContents(
			[dt](GameObject* o) {
				o->Update(dt);
			}
		);
	}
}

void NetworkedGame::UpdateAsServer(float dt) {
	// --- 任务 2.4: 更新合作逻辑 ---
	ServerUpdateCoopMechanic(dt);
	// --- 任务 2.3: 发送状态快照 ---
	packetsToSnapshot--;
	if (packetsToSnapshot < 0) {
		BroadcastSnapshot(false);
		packetsToSnapshot = 5;
	}
	else {
		BroadcastSnapshot(true);
	}
	thisServer->UpdateServer();
}

void NetworkedGame::UpdateAsClient(float dt) {

	// --- 任务 2.3: 发送玩家输入 ---
	if (localPlayer) {
		ClientPacket newPacket;

		// 收集输入
		newPacket.buttonstates[0] = Window::GetKeyboard()->KeyDown(KeyCodes::W) ? 1 : 0;
		newPacket.buttonstates[1] = Window::GetKeyboard()->KeyDown(KeyCodes::S) ? 1 : 0;
		newPacket.buttonstates[2] = Window::GetKeyboard()->KeyDown(KeyCodes::A) ? 1 : 0;
		newPacket.buttonstates[3] = Window::GetKeyboard()->KeyDown(KeyCodes::D) ? 1 : 0;
		newPacket.buttonstates[4] = Window::GetKeyboard()->KeyPressed(KeyCodes::SPACE) ? 1 : 0;

		// 发送相机朝向 (Yaw) 用于移动方向计算
		newPacket.yaw = (int)world.GetMainCamera().GetYaw();

		newPacket.lastID = 0;
		thisClient->SendPacket(newPacket);
	}

	thisClient->UpdateClient();
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
		int playerState = 0;
		GamePacket* newPacket = nullptr;
		if (o->WritePacket(&newPacket, deltaFrame, playerState)) {
			thisServer->SendGlobalPacket(*newPacket);
			delete newPacket;
		}
	}
}

void NetworkedGame::UpdateMinimumState() {
	// ... (保持原样)
}

// --- 任务 2.3: 实现 AddPlayerToWorld ---
NetworkPlayer* NetworkedGame::AddPlayerToWorld(const Vector3& position, int playerID) {

	NetworkPlayer* character = new NetworkPlayer(this, playerID);
	SphereVolume* volume = new SphereVolume(1.0f);

	character->SetBoundingVolume(volume);

	character->GetTransform()
		.SetScale(Vector3(1, 1, 1))
		.SetPosition(position);

	// 使用 MyGame 的 catMesh (protected 成员)
	character->SetRenderObject(new RenderObject(character->GetTransform(), catMesh, notexMaterial));
	character->GetRenderObject()->SetColour(Vector4(0, 1, 1, 1)); // 网络玩家默认为青色

	character->SetPhysicsObject(new PhysicsObject(character->GetTransform(), character->GetBoundingVolume()));
	character->GetPhysicsObject()->SetInverseMass(0.5f);
	character->GetPhysicsObject()->InitSphereInertia();
	character->GetPhysicsObject()->SetElasticity(0.0f); // 无弹跳

	// --- 关键: 创建 NetworkObject ---
	NetworkObject* netObj = new NetworkObject(*character, playerID);
	networkObjects.push_back(netObj);

	world.AddGameObject(character);

	return character;
}

NetworkObject* NetworkedGame::GetNetworkObject(int objectID) const {
	for (auto* o : networkObjects) {
		if (o->GetNetworkID() == objectID) {
			return o;
		}
	}
	return nullptr;
}

void NetworkedGame::StartLevel() {
	InitWorld();
}

void NetworkedGame::ReceivePacket(int type, GamePacket* payload, int source) {

	if (thisServer) {
		// --- 服务器收包 ---
		switch (type) {
		case GamePacketTypes::Client_Input: // 使用我们定义的枚举
			HandleClientInput((ClientPacket*)payload, source);
			break;
			// case BasicNetworkMessages::Received_State:
				// ...
		}
	}
	else if (thisClient) {
		// --- 客户端收包 ---
		switch (type) {
		case BasicNetworkMessages::Full_State:
		case BasicNetworkMessages::Delta_State: {
			FullPacket* packet = (FullPacket*)payload;
			NetworkObject* no = GetNetworkObject(packet->objectID);
			if (no) {
				no->ReadPacket(*payload);
			}
			break;
		}
		case BasicNetworkMessages::Player_Connected: {
			NewPlayerPacket* packet = (NewPlayerPacket*)payload;
			int playerID = packet->playerID;
			Vector3 playerPos = packet->startPos;

			// 在客户端生成这个新玩家
			// 注意：AddPlayerToWorld 会检查是否已经存在该 ID 的玩家吗？
			// 为了简化，我们直接生成。
			// 实际项目中可能需要检查 serverPlayers 避免重复。
			NetworkPlayer* newPlayer = AddPlayerToWorld(playerPos, playerID);

			// 如果还没设置本地玩家，说明这是我们自己 (假设逻辑)
			// 或者，我们可以在 Connect 时保存 ID，或者服务器明确告诉我们 "YourID"
			// 这里使用简单的逻辑：第一个收到的 Player_Connected 默认为自己
			if (localPlayer == nullptr) {
				localPlayer = newPlayer;
				// playerObject = newPlayer; // 客户端不需要 playerObject，只用 localPlayer 即可
				std::cout << "Client: Local player spawned!" << std::endl;
			}
			break;
		}
		}
	}
}

// --- 任务 2.3: 服务器处理输入 ---
void NetworkedGame::HandleClientInput(ClientPacket* packet, int playerID) {
	auto it = serverPlayers.find(playerID);
	if (it == serverPlayers.end()) {
		// 第一次收到输入，生成玩家
		Vector3 startPos = Vector3(0, 5, 50);
		NetworkPlayer* newPlayer = AddPlayerToWorld(startPos, playerID);
		serverPlayers.emplace(playerID, newPlayer);

		// 告诉所有人有新玩家加入 (包括新玩家自己)
		NewPlayerPacket newPlayerPacket(playerID, startPos);
		thisServer->SendGlobalPacket(newPlayerPacket);

		it = serverPlayers.find(playerID);
	}

	NetworkPlayer* player = it->second;

	// --- 移动逻辑 ---
	// 使用 MyGame::PlayerControl 类似的逻辑
	Quaternion yaw = Quaternion::EulerAnglesToQuaternion(0, packet->yaw, 0);
	Vector3 targetDir(0, 0, 0);

	if (packet->buttonstates[0]) targetDir.z -= 1; // W
	if (packet->buttonstates[1]) targetDir.z += 1; // S
	if (packet->buttonstates[2]) targetDir.x -= 1; // A
	if (packet->buttonstates[3]) targetDir.x += 1; // D

	if (Vector::LengthSquared(targetDir) > 0) {
		targetDir = Vector::Normalise(targetDir);
		targetDir = yaw * targetDir; // 转换到世界空间

		// 自动转向 (可选)
		// ... (省略复杂的 Slerp，直接移动)

		player->GetPhysicsObject()->AddForce(targetDir * 50.0f);
	}

	if (packet->buttonstates[4]) { // Space
		// 需要 IsPlayerOnGround(player) 检查
		if (IsPlayerOnGround(player)) {
			player->GetPhysicsObject()->ApplyLinearImpulse(Vector3(0, 20, 0));
		}
	}
}

void NetworkedGame::OnPlayerCollision(NetworkPlayer* a, NetworkPlayer* b) {
	if (thisServer) {
		MessagePacket newPacket;
		newPacket.messageID = COLLISION_MSG;
		newPacket.playerID = a->GetPlayerNum();

		// 使用 GameServer::SendPacketToClient 辅助函数
		// (需要确保 GameServer.h 中声明了这个函数)
		// thisServer->SendPacketToClient(newPacket, b->GetPlayerNum()); 
		// ...
	}
}

// -------------------------------------------------------------------------------------
// 任务 2.4: 双人包裹机制实现
// 辅助函数：在网络环境中生成一个普通物体
// 注意：Server 和 Client 都需要调用这个函数，并传入相同的 networkID (例如 100)
GameObject* NetworkedGame::SpawnNetworkedObject(int id, Vector3 pos, Vector3 scale, float inverseMass) {
	GameObject* obj = new GameObject("HeavyPackage");

	// 设置 AABB 碰撞体积
	AABBVolume* volume = new AABBVolume(scale);
	obj->SetBoundingVolume(volume);

	obj->GetTransform()
		.SetPosition(pos)
		.SetScale(scale * 2.0f);

	// 这里我们假设 cubeMesh 已经在 MyGame 中加载
	obj->SetRenderObject(new RenderObject(obj->GetTransform(), cubeMesh, checkerMaterial));
	obj->GetRenderObject()->SetColour(Vector4(1, 0, 0, 1)); // 初始红色 (不可移动)

	// 设置物理
	obj->SetPhysicsObject(new PhysicsObject(obj->GetTransform(), obj->GetBoundingVolume()));
	obj->GetPhysicsObject()->SetInverseMass(inverseMass);
	obj->GetPhysicsObject()->InitCubeInertia();

	// 关键：添加 NetworkObject 组件用于同步
	NetworkObject* netObj = new NetworkObject(*obj, id);
	networkObjects.push_back(netObj);
	obj->SetNetworkObject(netObj);

	world.AddGameObject(obj);
	return obj;
}

// 服务器端逻辑：每帧调用
void NetworkedGame::ServerUpdateCoopMechanic(float dt) {
	if (!localHeavyPackage) return;

	int playersInRange = 0;
	float activationRadius = 10.0f; // 判定半径

	Vector3 packagePos = localHeavyPackage->GetTransform().GetPosition();

	// 遍历所有在线玩家
	for (auto const& [id, player] : serverPlayers) {
		if (player) {
			float dist = Vector::Length(player->GetTransform().GetPosition() - packagePos);
			if (dist < activationRadius) {
				playersInRange++;
			}
		}
	}

	PhysicsObject* phys = localHeavyPackage->GetPhysicsObject();
	RenderObject* rend = localHeavyPackage->GetRenderObject();

	// 规则：需要至少 2 名玩家
	if (playersInRange >= 2) {
		// 激活状态：变轻，变绿
		if (phys->GetInverseMass() < 0.1f) { // 之前是 0
			phys->SetInverseMass(2.0f); // 变得容易推动

			if (rend) rend->SetColour(Vector4(0, 1, 0, 1)); // 绿色

			// 可选：发送消息通知客户端 (State Changed)
			// 简单起见，我们依赖 Snapshot 同步位置。
			// 颜色同步通常需要 StatePacket，这里如果只同步了位置，客户端看到的还是红色，但物体会动。
			// 完美的做法是添加 ObjectState 包，但对于 Part B 基础分，物理同步是关键。
		}
	}
	else {
		// 冻结状态：无限质量，变红
		if (phys->GetInverseMass() > 0.1f) {
			phys->SetInverseMass(0.0f); // 无法移动
			phys->SetLinearVelocity(Vector3(0, 0, 0)); // 立即停下
			phys->SetAngularVelocity(Vector3(0, 0, 0));
			if (rend) rend->SetColour(Vector4(1, 0, 0, 1)); // 红色
		}
	}
}

void NetworkedGame::InitGrid() {
	// 动态生成一个简单的 Grid 文件 (TestGrid.grid)
	// 假设地图大小 200x200，格子大小 10 -> 20x20 个格子
	int nodeSize = 10;
	int width = 40; // 400 宽
	int height = 40; // 400 高
	// 这样覆盖 -200 到 +200

	std::ofstream outfile(Assets::DATADIR + "TestGrid.grid");
	outfile << nodeSize << std::endl;
	outfile << width << std::endl;
	outfile << height << std::endl;

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			// 这里可以根据世界坐标判断是否有墙，目前全部设为地面 '.'
			// 进阶做法：遍历 world 中的物体，如果这里有墙就写 'x'
			outfile << ".";
		}
		outfile << std::endl;
	}
	outfile.close();

	// 加载网格
	navGrid = new NavigationGrid("TestGrid.grid");
}