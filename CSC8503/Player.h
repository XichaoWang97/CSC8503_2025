#pragma once
#include "GameCharacter.h"
#include "Window.h"
#include "RenderObject.h"
using namespace NCL;

namespace NCL::CSC8503 {
    class Rendering::Mesh;

    // define a struct about player input
    struct PlayerInputs {
        bool isMoving = false;
        bool jump = false;
        bool attack = false; // Grab/Throw
        Vector3 axis = Vector3(0, 0, 0);
        float cameraYaw = 0.0f;
    };

    class Player : public GameCharacter {
    public:
        Player(GameWorld* world);
        ~Player();

        void Update(float dt) override;
        void SetIgnoreInput(bool ignore) { ignoreInput = ignore; } // We should ignore input in NetworkedGame Mode
        void SetPlayerInput(const PlayerInputs& inputs) { currentInputs = inputs; } // for networked game

    private:
        virtual void PlayerControl(float dt);
        bool IsPlayerOnGround();
        bool ignoreInput = false;
        PlayerInputs currentInputs;
    };
}