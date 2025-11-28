#pragma once
#include "MyGame.h"
#include "NetworkBase.h"
#include "NetworkState.h"
#include "NetworkObject.h"
#include "NavigationGrid.h"
#include "GooseNPC.h"
#include "RivalAI.h"

namespace NCL::CSC8503 {
	class GameServer;
	class GameClient;
	class NetworkPlayer;
	class NetworkObject;

	// --- 任务 2.3: 定义游戏专属数据包类型 ---
	// 我们在这里扩展 BasicNetworkMessages
	enum GamePacketTypes {
		Client_Input = BasicNetworkMessages::Shutdown + 1, // 从 Shutdown 之后开始计数
		// 可以在这里添加更多类型，如 Client_Fire, Server_Spawn_Item 等
	};

	class NetworkedGame : public MyGame, public PacketReceiver
	{
	public:
		NetworkedGame(GameWorld& gameWorld, GameTechRendererInterface& renderer, PhysicsSystem& physics);
		~NetworkedGame();

		void StartAsServer();
		void StartAsClient(char a, char b, char c, char d);

		void UpdateGame(float dt) override;

		// --- 任务 2.3: 添加新的玩家生成函数 ---
		NetworkPlayer* AddPlayerToWorld(const Vector3& position, int playerID);

		// --- 任务 2.3: 新增辅助函数 ---
		NetworkObject* GetNetworkObject(int objectID) const;

		void StartLevel();

		void ReceivePacket(int type, GamePacket* payload, int source) override;

		void OnPlayerCollision(NetworkPlayer* a, NetworkPlayer* b);

	protected:
		void UpdateAsServer(float dt);
		void UpdateAsClient(float dt);

		// --- 任务 2.3: 新增 - 服务器处理客户端输入 ---
		void HandleClientInput(ClientPacket* packet, int playerID);

		void BroadcastSnapshot(bool deltaFrame);
		void UpdateMinimumState();
		std::map<int, int> stateIDs;

		GameServer* thisServer;
		GameClient* thisClient;
		float timeToNextPacket;
		int packetsToSnapshot;

		std::vector<NetworkObject*> networkObjects;

		// --- 任务 2.3: 修改 - 使用 NetworkPlayer* ---
		std::map<int, NetworkPlayer*> serverPlayers;
		NetworkPlayer* localPlayer; // 客户端本地玩家
		int myPlayerID; // 客户端 ID

		// --- 任务 2.4 新增: 双人合作机制 ---
		GameObject* localHeavyPackage = nullptr; // 指向场景中的重包裹
		void ServerUpdateCoopMechanic(float dt); // 服务器专用：检测玩家距离并调整包裹质量

		// 辅助函数：生成网络物体（非玩家）
		GameObject* SpawnNetworkedObject(int id, Vector3 pos, Vector3 scale, float inverseMass);

		// --- 任务 3.1 新增: 鹅 NPC 和网格 ---
		NavigationGrid* navGrid = nullptr;
		GooseNPC* angryGoose = nullptr;

		void InitGrid(); // 新增初始化函数
		RivalAI* rivalAI = nullptr; // 竞争对手 AI
	};
}