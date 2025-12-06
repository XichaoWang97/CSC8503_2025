#pragma once
#include "MyGame.h"
#include "NetworkBase.h"
#include "NetworkState.h"
#include "NetworkObject.h"
#include "NavigationGrid.h"

namespace NCL::CSC8503 {
	class GameServer;
	class GameClient;
	class NetworkPlayer;

	class NetworkedGame : public MyGame, public PacketReceiver {
	public:
		NetworkedGame(GameWorld& gameWorld, GameTechRendererInterface& renderer, PhysicsSystem& physics);
		~NetworkedGame();

		void StartAsServer();
		void StartAsClient(char a, char b, char c, char d);

		void UpdateGame(float dt) override;

		// 网络包接收回调
		void ReceivePacket(int type, GamePacket* payload, int source) override;

		// 处理玩家连接/断开
		void OnPlayerCollision(NetworkPlayer* a, NetworkPlayer* b);

	protected:
		void UpdateAsServer(float dt);
		void UpdateAsClient(float dt);

		void BroadcastSnapshot(bool deltaFrame);
		void UpdateMinimumState();

		// 生成网络玩家
		GameObject* SpawnPlayer(int playerID, bool isSelf = false);
		void ServerProcessClientInput(int playerID, ClientPacket* packet);

		GameServer* thisServer;
		GameClient* thisClient;
		float timeToNextPacket;
		int packetsToSnapshot;

		std::map<int, int> stateIDs; // 记录每个客户端确认的最新状态ID
		std::map<int, GameObject*> serverPlayers; // Server端: ClientID -> PlayerObject
		GameObject* localPlayer; // Client端: 自己的玩家对象
	};
}