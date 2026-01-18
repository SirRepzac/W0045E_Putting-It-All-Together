#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <algorithm>
#include "Constants.h"
#include "Movable.h"
#include "Logger.h"
#include "Grid.h"


class AIBrain;
class Behaviour;

class GameAI : public Movable
{
public:

	enum State
	{
		STATE_IDLE,
		STATE_SEEK,
		STATE_FLEE,
		STATE_ARRIVE,
		STATE_WANDER,
		STATE_EVADE,
		STATE_PURSUE,
		STATE_FOLLOW_PATH
	};

	GameAI(Vec2 pos);
	~GameAI();

	void SetState(State state);

	const std::string& GetName() { return name; };

	void Update(float deltaTime) override;

	bool CanUseNode(const PathNode* node) const;

	bool CanGoTo(PathNode* destination);

	void GoTo(PathNode* destination, bool& isPathValid, bool ignoreFog = false);

	void GoToClosest(PathNode::ResourceType destinationType, bool& isPathValid);

	void GoToClosest(std::vector<PathNode::ResourceType> destinationTypes, bool& isPathValid);

	void SetTarget(Vec2 target) { targetPos = target; }
	void SetTarget(Movable* target) { targetMovable = target; }
	Vec2 GetTarget() { return targetPos; }
	Movable* GetMovingTarget() { return targetMovable; }
	State GetCurrentState() { return currentState; }

	AIBrain* GetBrain() { return brain; }

	Vec2 GetPrevPos() { return prevPos; }


	PathNode* GetPathDestination();

private:
	Vec2 targetPos;
	Movable* targetMovable = nullptr;

	Vec2 prevPos;
	State currentState;

	Behaviour* behaviour;

	float BEHAVIOUR_WEIGHT = 1;
	float SEPARATION_WEIGHT = 0; // recommended value: 2.0
	float AGENTAVOIDANCE_WEIGHT = 0.0f; // recommended value: 2.0
	float WALLAVOIDANCE_WEIGHT = 0.0f; // recommended value: 1.0

	AIBrain* brain;

	PathNode* desiredDestination = nullptr;
};

static inline const std::string ToString(GameAI::State s)
{
	switch (s)
	{
	case GameAI::State::STATE_IDLE:        return "Idle";
	case GameAI::State::STATE_SEEK:        return "Seek";
	case GameAI::State::STATE_FLEE:        return "Flee";
	case GameAI::State::STATE_ARRIVE:      return "Arrive";
	case GameAI::State::STATE_WANDER:      return "Wander";
	case GameAI::State::STATE_EVADE:       return "Evade";
	case GameAI::State::STATE_PURSUE:      return "Pursue";
	case GameAI::State::STATE_FOLLOW_PATH: return "Follow Path";
	default:                       return "Unknown";
	}
}