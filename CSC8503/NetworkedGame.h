#pragma once
#include "MyGame.h"
#include "NetworkBase.h"

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
	// --- 任务 2.3: 定义缺失的数据包结构体 ---
	// 这些通常应该在 NetworkBase.h 中，但为了不修改框架，我们定义在这里
	struct FullPacket : public GamePacket {
		int		objectID = -1;
		NetworkState fullState;

		FullPacket() {
			type = BasicNetworkMessages::Full_State;
			size = sizeof(FullPacket) - sizeof(GamePacket);
		}
	};

	struct DeltaPacket : public GamePacket {
		int		fullID = -1;
		int		objectID = -1;
		char	pos[3];
		char	orientation[4];

		DeltaPacket() {
			type = BasicNetworkMessages::Delta_State;
			size = sizeof(DeltaPacket) - sizeof(GamePacket);
		}
	};

	struct ClientPacket : public GamePacket {
		int		lastID;
		char	buttonstates[8]; // WASD, Space, etc.
		int     yaw;             // 摄像机朝向

		ClientPacket() {
			type = GamePacketTypes::Client_Input; // 使用我们自定义的类型
			size = sizeof(ClientPacket) - sizeof(GamePacket);
		}
	};

	struct NewPlayerPacket : public GamePacket {
		int playerID = -1;
		Vector3 startPos;
		NewPlayerPacket(int p = -1, Vector3 pos = Vector3()) {
			type = BasicNetworkMessages::Player_Connected;
			size = sizeof(NewPlayerPacket) - sizeof(GamePacket);
			playerID = p;
			startPos = pos;
		}
	};

	struct PlayerDisconnectPacket : public GamePacket {
		int playerID = -1;
		PlayerDisconnectPacket(int p = -1) {
			type = BasicNetworkMessages::Player_Disconnected;
			size = sizeof(PlayerDisconnectPacket) - sizeof(GamePacket);
			playerID = p;
		}
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
	};
}