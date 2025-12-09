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

		void StartAsServer(int playerCount);
		void StartAsClient(char a, char b, char c, char d);

		void UpdateGame(float dt) override;
		void ReceivePacket(int type, GamePacket* payload, int source) override;
		void OnPlayerCollision(NetworkPlayer* a, NetworkPlayer* b);
		void Disconnect();

		void PackageLogic(Player* player, float dt) override;
		void RivalLogic() override;
		void WinLoseLogic(Player* player) override;
		void GetCoinLogic(Player* player, float dt) override;

		void BroadcastHighScores();
		GameServer* GetServer() const { return thisServer; }

	protected:
		void UpdateAsServer(float dt);
		void UpdateAsClient(float dt);

		void BroadcastSnapshot(bool deltaFrame);
		void UpdateMinimumState();
		void InitDefaultPlayer() override;
		void InitNetworkObjectToWorld(); // dietribute network object
		void UpdateKeys() override;
		void DrawNetworkHUD(); // draw UI to show player states

		// generate player object for a given client ID
		GameObject* SpawnNetworkedPlayer(int playerID);
		void ServerProcessClientInput(int playerID, ClientPacket* packet);
		
		GameServer* thisServer;
		GameClient* thisClient;
		float timeToNextPacket;
		int packetsToSnapshot;

		// player states
		bool ui_p2Connected = false;
		bool ui_p1Dead = false;
		bool ui_p2Dead = false;
		bool isP2ConnectedServer = false;

		float jumpTimer = 0.0f;
		std::map<int, bool> lastJumpState;
		std::map<int, int> stateIDs; // record last acknowledged state ID per client
		std::map<int, GameObject*> serverPlayers; // ClientID -> PlayerObject
	};
}