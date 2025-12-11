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
        BehaviourState HasHighScore(float dt);      // 条件：分数够了吗？
        BehaviourState IsHoldingPackage(float dt);  // 条件：我拿着包裹吗？
        BehaviourState IsHoldingStone(float dt);    // 条件：我拿着石头吗？
        BehaviourState DoesPlayerHavePackage(float dt); // 条件：玩家拿着包裹吗？

        BehaviourState FindWinZone(float dt);       // 动作：去终点
        BehaviourState GetClosestStone(float dt);   // 动作：找石头并设定目标
        BehaviourState GetClosestCoin(float dt);    // 动作：找金币并设定目标
        BehaviourState GetPackageOrCamp(float dt);  // 动作：找包裹或蹲点

        BehaviourState MoveToTarget(float dt);      // 通用动作：移动到 currentTarget
        BehaviourState AttemptGrab(float dt);       // 通用动作：尝试抓取 currentTarget
        BehaviourState ThrowAtPlayer(float dt);     // 通用动作：向玩家扔石头
		Player* FindPlayerHoldingPackage();         // find player who holds package
        

        GameObject* exitPoint; // 【新增】存储撤离点坐标
        GameWorld* gameWorld;
        NavigationGrid* grid;
        std::vector<Player*>* allPlayers;
		GameObject* currentTarget;  // objective target

		Vector3 packageSpawnPos;    // package camp position(pakcage rebirth point)
		BehaviourNode* rootNode;
        NavigationPath currentPath;
        std::vector<Vector3> pathPoints;
        Vector3 lastCalcTargetPos; // 记录上一次寻路时的目标位置

        int currentScore;
        int rivalWinningScore;
        float moveSpeed;

		bool IsOnGround();
        float jumpCooldown = 0.0f;
        float timeSinceLastPathCalc;
    };
}