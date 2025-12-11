#pragma once
#include "StateGameObject.h" // 保持继承 StateGameObject 以兼容 MyGame 中的指针，但我们不使用它的状态机
#include "NavigationGrid.h"
#include "BehaviourNode.h"
#include "BehaviourSelector.h"
#include "BehaviourSequence.h"
#include "BehaviourAction.h"
#include "Player.h"

namespace NCL::CSC8503 {
    class GooseNPC : public StateGameObject {
    public:
        GooseNPC(NavigationGrid* grid);
        ~GooseNPC();

        void Update(float dt) override;

    protected:
		// Behaviour Tree
        void BuildBehaviourTree();

        BehaviourState ChasePlayer(float dt);

        // 辅助函数
        void CalculatePathTo(Vector3 targetPos);
        void LookAt(Vector3 targetPos, float dt); // 加入 dt 以实现平滑转向

        NavigationGrid* grid;

        BehaviourNode* rootNode;

        NavigationPath  currentPath;
        std::vector<Vector3> pathPoints;

        Vector3 lastCalcTargetPos; // 记录上次寻路的目标位置
        float timeSinceLastPathCalc = 0.0f;
        float chaseSpeed;
    };
}