#pragma once
#include "StateGameObject.h"
#include "NavigationGrid.h"

namespace NCL::CSC8503 {
    class GooseNPC : public StateGameObject {
    public:
        GooseNPC(NavigationGrid* grid, GameObject* player);
        ~GooseNPC();

        void Update(float dt) override;

    protected:
        // AI behaviour
        void ChasePlayer(float dt);
        void Wander(float dt);

        // path
        void CalculatePathTo(Vector3 targetPos);
        void FollowPath(float dt);

        NavigationGrid* grid;
        NavigationPath  currentPath;
        std::vector<Vector3> pathPoints;

        float timeSinceLastPathCalc = 0.0f;
    };
}

