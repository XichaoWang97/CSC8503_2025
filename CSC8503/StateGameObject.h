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

            void SetTarget(Player* target) { this->playerTarget = target; }
            void SetPatrolPath(const std::vector<Vector3>& path) { this->patrolPath = path; }

            void SetGameWorld(GameWorld* gw) { this->gameWorld = gw; }

            virtual void OnCollisionBegin(GameObject* otherObject) override;

            void SetPlayerList(std::vector<Player*>* players) { allPlayers = players; }
            Player* GetClosestPlayer();

        protected:

            void Patrol(float dt);
            void Chase(float dt);
            void MoveTo(Vector3 position, float speed, float dt);

            bool CanSeeTarget();

            StateMachine* stateMachine;
            float counter;

            std::vector<Player*>* allPlayers;

            Player* playerTarget = nullptr;
            GameWorld* gameWorld = nullptr;

            std::vector<Vector3> patrolPath;
            int currentWaypointIndex = 0;

            float patrolSpeed = 5.0f;
            float chaseSpeed = 12.0f;
        };
    }
}
