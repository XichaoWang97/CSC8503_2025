#pragma once
#include "GameObject.h"

namespace NCL {
    namespace CSC8503 {
        class StateMachine;
        class StateGameObject : public GameObject  
        {
        public:
            StateGameObject(const std::string& name = "");
            ~StateGameObject();

            virtual void Update(float dt);

            // --- 任务 1.1: AI 设置接口 ---
            void SetTarget(GameObject* target) { this->playerTarget = target; }
            void SetPatrolPath(const std::vector<Vector3>& path) { this->patrolPath = path; }

        protected:
            // --- 任务 1.1: 状态行为函数 ---
            void Patrol(float dt);
            void Chase(float dt);
            void MoveTo(Vector3 position, float speed, float dt); // 辅助移动函数

            StateMachine* stateMachine;
            float counter;

            // AI 数据
            GameObject* playerTarget = nullptr;
            std::vector<Vector3> patrolPath;
            int currentWaypointIndex = 0;

            float patrolSpeed = 5.0f;
            float chaseSpeed = 12.0f;
        };
    }
}
