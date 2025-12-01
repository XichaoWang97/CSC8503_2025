#pragma once
#include "GameCharacter.h"
#include "BehaviourNode.h"
#include "NavigationGrid.h"
#include "NavigationPath.h"
#include "GameWorld.h"

namespace NCL::CSC8503 {
    class RivalAI : public GameCharacter {
    public:
        RivalAI(GameWorld* world, NavigationGrid* _grid);
        ~RivalAI();

        void Update(float dt) override;
        void SetGameWorld(GameWorld* gw) { gameWorld = gw; }
        // 在行为树里调用父类方法
        /*BehaviourState AttackAction(float dt) {
            // ... 瞄准逻辑 ...
            Vector3 targetDir = (targetPos - GetTransform().GetPosition()).Normalised();

            ThrowHeldItem(targetDir); // <--- 直接调用父类方法！
            return Success;
        }

        BehaviourState PickupAction(float dt) {
            // ...
            Vector3 dirToStone = (stonePos - GetTransform().GetPosition()).Normalised();
            TryGrab(dirToStone); // <--- 直接调用父类方法！
            return Success;
        }*/
    protected:
        // --- 行为树相关函数 ---
        void BuildBehaviourTree();

        // 行为逻辑
        BehaviourState FindTargetPacket(float dt); // 寻找目标
        BehaviourState MoveToTarget(float dt);     // 移动
        BehaviourState LookAtTarget(float dt);     // 瞄准
        BehaviourState ThrowStone(float dt);       // 攻击 (任务 3.4)

        // 辅助函数
        void CalculatePath();

        GameWorld* gameWorld;
        NavigationGrid* grid;
        GameObject* currentTarget; // 当前想要攻击的包裹

        BehaviourNode* rootNode;

        // 寻路数据
        NavigationPath currentPath;
        std::vector<Vector3> pathPoints;
        float moveSpeed;
    };
}
