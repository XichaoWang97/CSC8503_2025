#pragma once
#include "GameCharacter.h"
#include "Window.h"
#include "RenderObject.h"
using namespace NCL;

namespace NCL::CSC8503 {
    class Rendering::Mesh;
    class Player : public GameCharacter {
    public:
        Player(GameWorld* world);
        ~Player();

        void Update(float dt) override;

    private:
        virtual void PlayerControl(float dt);
        bool IsPlayerOnGround();
    };
}