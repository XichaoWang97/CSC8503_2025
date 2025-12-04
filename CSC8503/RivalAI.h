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

        // 设置必要的外部引用
        void SetPlayer(GameCharacter* p) { player = p; }
        void SetPackageSpawn(Vector3 pos) { packageSpawnPos = pos; }

        // 外部系统调用此函数增加AI分数
        void AddScore(int amount) { currentScore += amount; }
        int  GetScore() const { return currentScore; }

    protected:
        void BuildBehaviourTree();

        // --- 辅助感知函数 ---
        GameObject* FindClosestObject(std::string name); // 找石头或金币
        GameObject* FindPackage(); // 特指找包裹
        bool IsProneToBreak(GameObject* obj); // 判断目标是否是易碎品

        // --- 行为逻辑函数 ---
        void CalculatePath(Vector3 targetPos);
        void LookAt(Vector3 targetPos); // 转向目标以便投掷或抓取
        void Jump(); // 执行跳跃
        void CheckObstaclesAndJump(float dt); // 检测前方障碍并决定是否跳跃
        // --- 行为树 Action ---
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
        GameCharacter* player;      // 玩家引用
        GameObject* currentTarget;  // 当前关注的物体（石头/金币/包裹）

        Vector3 packageSpawnPos;    // 包裹重生点
        BehaviourNode* rootNode;

        NavigationPath currentPath;
        std::vector<Vector3> pathPoints;

        int currentScore;
        int winningScore;
        float moveSpeed;

		bool IsOnGround(); // check if AI is on the ground
        float jumpCooldown = 0.0f; // 跳跃冷却，防止连跳飞天
        float timeSinceLastPathCalc;
    };
}