#pragma once
#include "StateGameObject.h"
#include "BehaviourNode.h"
#include "BehaviourSelector.h"
#include "BehaviourSequence.h"
#include "BehaviourAction.h"
#include "NavigationGrid.h"
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

        // Helper functions
        void CalculatePathTo(Vector3 targetPos);
        void LookAt(Vector3 targetPos, float dt); // Add dt to achieve smooth rotation

        NavigationGrid* grid;

        BehaviourNode* rootNode;

        NavigationPath  currentPath;
        std::vector<Vector3> pathPoints;

        Vector3 lastCalcTargetPos; // Record the target position of the last pathfinding
        float timeSinceLastPathCalc = 0.0f;
        float chaseSpeed;
    };
}