#pragma once
#include "BehaviourNodeWithChildren.h"

class BehaviourSelector: public BehaviourNodeWithChildren {
public:
	BehaviourSelector(const std::string& nodeName) : BehaviourNodeWithChildren(nodeName) {}
	~BehaviourSelector() {}
	BehaviourState Execute(float dt) override {
		//std::cout << "Executing selector " << name << "\n";
		for (auto& i : childNodes) {
			BehaviourState nodeState = i->Execute(dt);
			switch (nodeState) {
				case Failure: continue;
				case Success:
				case Ongoing:
				{
					currentState = nodeState;
					return currentState;
				}
			}
		}
		return Failure;
	}
};

/*class BehaviourSelector : public BehaviourNode {
public:
    BehaviourSelector(const std::string& nodeName) : BehaviourNode(nodeName) {}
    ~BehaviourSelector() {
        for (auto& i : childNodes) delete i;
    }
    void AddChild(BehaviourNode* n) { childNodes.emplace_back(n); }

    BehaviourState Execute(float dt) override {
        for (auto& i : childNodes) {
            BehaviourState nodeState = i->Execute(dt);
            switch (nodeState) {
			    case Failure: continue;
			    case Success:
			    case Ongoing:
			    {
			    	currentState = nodeState;
			    	return currentState;
			    }
            }
        }
        return Failure;
    }
    void Reset() override {
        currentState = Initialise;
        for (auto& i : childNodes) i->Reset();
    }
protected:
    std::vector<BehaviourNode*> childNodes;
};*/