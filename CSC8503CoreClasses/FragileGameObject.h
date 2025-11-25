#pragma once
#include "GameObject.h"

namespace NCL {
    namespace CSC8503 {
        class FragileGameObject : public GameObject {
        public:
            FragileGameObject(const std::string& name = "");
            ~FragileGameObject();

            // 重写碰撞回调，用于检测伤害
            virtual void OnCollisionBegin(GameObject* otherObject) override;

            bool IsBroken() const { return isBroken; }
            float GetHealth() const { return health; }

        protected:
            float health;
            float maxHealth;
            bool isBroken;
        };
    }
}