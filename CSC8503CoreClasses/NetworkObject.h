#pragma once
#include "GameObject.h"
#include "NetworkBase.h"
#include "NetworkState.h"

namespace NCL::CSC8503 {
	class GameObject;

	struct FullPacket : public GamePacket {
		int		objectID = -1;
		NetworkState fullState;

		FullPacket() {
			type = Full_State;
			size = sizeof(FullPacket) - sizeof(GamePacket);
		}
	};

	struct DeltaPacket : public GamePacket {
		int		fullID		= -1;
		int		objectID	= -1;
		char	pos[3];
		char	orientation[4];

		DeltaPacket() {
			type = Delta_State;
			size = sizeof(DeltaPacket) - sizeof(GamePacket);
		}
	};

	struct ClientPacket : public GamePacket {
		int		lastID;
		char	buttonstates[8]; // 0: Space(Jump), 1: MouseLeft(Shoot), etc.
		int     axis[2]; // [0]: Forward/Back (W/S), [1]: Left/Right (A/D)
		float   yaw; // for camera control
		ClientPacket() {
			type = BasicNetworkMessages::Client_Update; // Client_Update is in the enum of NetworkBase.h
			size = sizeof(ClientPacket) - sizeof(GamePacket);
			lastID = 0;
			axis[0] = 0;
			axis[1] = 0;
			yaw = 0.0f;
			memset(buttonstates, 0, 8);
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

	class NetworkObject		{
	public:
		NetworkObject(GameObject& o, int id);
		virtual ~NetworkObject();

		//Called by clients
		virtual bool ReadPacket(GamePacket& p);
		//Called by servers
		virtual bool WritePacket(GamePacket** p, bool deltaFrame, int stateID);

		void UpdateStateHistory(int minID);
		int GetNetworkID() { return networkID; }

	protected:
		NetworkState& GetLatestNetworkState();

		bool GetNetworkState(int frameID, NetworkState& state);

		virtual bool ReadDeltaPacket(DeltaPacket &p);
		virtual bool ReadFullPacket(FullPacket &p);

		virtual bool WriteDeltaPacket(GamePacket**p, int stateID);
		virtual bool WriteFullPacket(GamePacket**p);

		GameObject& object;

		NetworkState lastFullState;

		std::vector<NetworkState> stateHistory;

		int deltaErrors;
		int fullErrors;

		int networkID;
	};
}