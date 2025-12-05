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
using namespace NCL;

namespace NCL::CSC8503 {
    class RivalAI : public GameCharacter {
    public:
        RivalAI(GameWorld* world, NavigationGrid* _grid);
        ~RivalAI();

        void Update(float dt) override;

        void SetPlayer(GameCharacter* p) { player = p; }
        void SetPackageSpawn(Vector3 pos) { packageSpawnPos = pos; }

		// AI add/get score
        void AddScore(int amount) { currentScore += amount; }
        int  GetScore() const { return currentScore; }

    protected:
        void BuildBehaviourTree();

        GameObject* FindClosestObject(std::string name);
        GameObject* FindPackage();

        void CalculatePath(Vector3 targetPos);
		void LookAt(Vector3 targetPos); // look at target position
        void Jump();

		// Behaviour Nodes:
        BehaviourState HasHighScore(float dt);      // 条件：分数够了吗？
        BehaviourState IsHoldingPackage(float dt);  // 条件：我拿着包裹吗？
        BehaviourState IsHoldingStone(float dt);    // 条件：我拿着石头吗？
        BehaviourState DoesPlayerHavePackage(float dt); // 条件：玩家拿着包裹吗？

        BehaviourState RunAwayFromPlayer(float dt); // 动作：逃跑
        BehaviourState GetClosestStone(float dt);   // 动作：找石头并设定目标
        BehaviourState GetClosestCoin(float dt);    // 动作：找金币并设定目标
        BehaviourState GetPackageOrCamp(float dt);  // 动作：找包裹或蹲点

        BehaviourState MoveToTarget(float dt);      // 通用动作：移动到 currentTarget
        BehaviourState AttemptGrab(float dt);       // 通用动作：尝试抓取 currentTarget
        BehaviourState ThrowAtPlayer(float dt);     // 通用动作：向玩家扔石头

        GameWorld* gameWorld;
        NavigationGrid* grid;
        GameCharacter* player;
		GameObject* currentTarget;  // objective target

		Vector3 packageSpawnPos;    // package camp position(pakcage rebirth point)
		BehaviourNode* rootNode;
        NavigationPath currentPath;
        std::vector<Vector3> pathPoints;

        int currentScore;
        int winningScore;
        float moveSpeed;

		bool IsOnGround();
        float jumpCooldown = 0.0f;
        float timeSinceLastPathCalc;
    };
}