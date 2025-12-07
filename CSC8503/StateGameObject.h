#pragma once
#include "GameObject.h"
#include "Player.h"

namespace NCL {
    namespace CSC8503 {
        class StateMachine;
        class GameWorld;
        class StateGameObject : public GameObject  
        {
        public:
            StateGameObject(const std::string& name = "");
            ~StateGameObject();

            virtual void Update(float dt);

            // --- 任务 1.1: AI 设置接口 ---
            void SetTarget(GameObject* target) { this->playerTarget = target; }
            void SetPatrolPath(const std::vector<Vector3>& path) { this->patrolPath = path; }
            // --- 任务 1.2: 注入 GameWorld ---
            void SetGameWorld(GameWorld* gw) { this->gameWorld = gw; }
            // --- 任务 1.3 新增: 设置玩家重置点 ---
            void SetResetPoint(const Vector3& pos) { this->resetPoint = pos; }
            // --- 任务 1.3 修改: 使用物理碰撞回调 ---
            virtual void OnCollisionBegin(GameObject* otherObject) override;

            void SetPlayerList(std::vector<Player*>* players) { allPlayers = players; }
            Player* GetClosestPlayer();

        protected:
            // --- 任务 1.1: 状态行为函数 ---
            void Patrol(float dt);
            void Chase(float dt);
            void MoveTo(Vector3 position, float speed, float dt); // 辅助移动函数

            // --- 任务 1.2: 视线检测函数 ---
            bool CanSeeTarget();

            StateMachine* stateMachine;
            float counter;

            std::vector<Player*>* allPlayers;
            // AI 数据
            GameObject* playerTarget = nullptr;
            GameWorld* gameWorld = nullptr; // 用于射线检测

            std::vector<Vector3> patrolPath;
            int currentWaypointIndex = 0;

            float patrolSpeed = 5.0f;
            float chaseSpeed = 12.0f;

            // --- 任务 1.3 新增 ---
            Vector3 resetPoint = Vector3(0, 0, 0);
        };
    }
}
