#pragma once
#include "GameCharacter.h"
#include "RenderObject.h"
#include "BehaviourNode.h"
#include "BehaviourSelector.h"
#include "BehaviourSequence.h"
#include "BehaviourAction.h"
#include "NavigationGrid.h"
#include "NavigationPath.h"
#include "GameWorld.h"
#include "Player.h"
using namespace NCL;

namespace NCL::CSC8503 {
    class RivalAI : public GameCharacter {
    public:
        RivalAI(GameWorld* world, NavigationGrid* _grid);
        ~RivalAI();

        void Update(float dt) override;

        void SetPlayerList(std::vector<Player*>* players) { allPlayers = players; }
        void SetPackageSpawn(Vector3 pos) { packageSpawnPos = pos; }

        // AI add/get score
        void SetScore(int amount) { currentScore = amount; }
        int  GetScore() const { return currentScore; }
        void SetWinningScore(int amount) { rivalWinningScore = amount; }

    protected:
        void BuildBehaviourTree();

        GameObject* FindClosestObject(std::string name);
        GameObject* FindPackage();

        void CalculatePath(Vector3 targetPos);
        void LookAt(Vector3 targetPos); // look at target position
        void Jump();
        float CalculatePathDistance(Vector3 startPos, Vector3 endPos);

        // Behaviour Nodes:
        BehaviourState HasHighScore(float dt);      // Condition: Is score enough?
        BehaviourState IsHoldingPackage(float dt);  // Condition: Am I holding the package?
        BehaviourState IsHoldingStone(float dt);    // Condition: Am I holding a stone?
        BehaviourState DoesPlayerHavePackage(float dt); // Condition: Is player holding the package?

        BehaviourState FindWinZone(float dt);       // Action: Go to extraction zone
        BehaviourState GetClosestStone(float dt);   // Action: Find stone and set target
        BehaviourState GetClosestCoin(float dt);    // Action: Find coin and set target
        BehaviourState GetPackageOrCamp(float dt);  // Action: Find package or camp

        BehaviourState MoveToTarget(float dt);      // Generic Action: Move to currentTarget
        BehaviourState AttemptGrab(float dt);       // Generic Action: Attempt to grab currentTarget
        BehaviourState ThrowAtPlayer(float dt);     // Generic Action: Throw stone at player
        Player* FindPlayerHoldingPackage();         // find player who holds package


        GameObject* exitPoint; // Store extraction point coordinates
        GameWorld* gameWorld;
        NavigationGrid* grid;
        std::vector<Player*>* allPlayers;
        GameObject* currentTarget;  // objective target

        Vector3 packageSpawnPos;    // package camp position(pakcage rebirth point)
        BehaviourNode* rootNode;
        NavigationPath currentPath;
        std::vector<Vector3> pathPoints;
        Vector3 lastCalcTargetPos; // Record the target position from the last path calculation

        int currentScore;
        int rivalWinningScore;
        float moveSpeed;

        bool IsOnGround();
        float jumpCooldown = 0.0f;
        float timeSinceLastPathCalc;
    };
}