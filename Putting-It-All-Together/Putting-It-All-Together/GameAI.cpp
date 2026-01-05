#include <iostream>
#include "GameAI.h"
#include "GameLoop.h"
#include "Behaviour.h"
#include "AIBrain.h"
#include "PathNode.h"


GameAI::GameAI(Vec2 pos) :
	currentState(State::STATE_IDLE),
	brain(new AIBrain(this))
{
	if (!pos)
		pos = Vec2(0, 0);

	behaviour = new Behaviour(this);
	velocity = Vec2(0.0f, 0.0f);
	direction = Vec2(0.0f, 1.0f);
	position = pos;
	prevPos = position;

	static int aiCounter = 1;
	name = "AI_" + std::to_string(aiCounter);
	aiCounter++;

	color = 0xC800C8;
}

GameAI::~GameAI()
{
	if (behaviour)
		delete behaviour;
	
	if (brain)
		delete brain;
	
}

void GameAI::SetState(State state)
{
	if (state != currentState)
		Logger::Instance().Log("State changed to: " + ToString(state) + "\n");
	currentState = state;
}

void GameAI::Update(float deltaTime)
{
	brain->Think(deltaTime);

	Behaviour::Info cInfo = Behaviour::Info();
	Behaviour::Info wInfo = Behaviour::Info();
	Behaviour::Info sInfo = Behaviour::Info();

	Behaviour::Info bInfo = behaviour->Update(deltaTime, currentState);

	if (AGENTAVOIDANCE_WEIGHT > 0)
		cInfo = behaviour->AgentAvoidance(deltaTime, currentState);
	if (WALLAVOIDANCE_WEIGHT > 0)
		wInfo = behaviour->WallAvoidance(deltaTime, currentState);
	if (SEPARATION_WEIGHT > 0)
		sInfo = behaviour->Separation(deltaTime, currentState);

	Behaviour::Info info; // merge all other info


	Vec2 steering =
		bInfo.direction * bInfo.acceleration * BEHAVIOUR_WEIGHT +
		sInfo.direction * sInfo.acceleration * SEPARATION_WEIGHT +
		cInfo.direction * cInfo.acceleration * AGENTAVOIDANCE_WEIGHT + 
		wInfo.direction * wInfo.acceleration * WALLAVOIDANCE_WEIGHT;

	if (steering.Length() > MAXIMUM_ACCELERATION)
		steering = steering.Normalized() * MAXIMUM_ACCELERATION;

	Move(steering.Normalized(), steering.Length(), deltaTime);

	prevPos = position;
	
}

void GameAI::GoTo(PathNode* destination)
{
	if (!destination)
		return;

	if (GetPathDestination() != nullptr)
		return;

	GameLoop& game = GameLoop::Instance();
	Pathfinder* pathfinder = game.pathfinder;
	PathNode* currNode = game.GetGrid().GetNodeAt(position);
	PathNode* dest = destination;
	float pathDist = 0;
	std::vector<PathNode*> path;
	path = pathfinder->RequestPath(currNode, dest, pathDist, radius);

	if (path.empty())
		return;

	SetState(State::STATE_FOLLOW_PATH);
	behaviour->SetPath(path);

}

void GameAI::GoToClosest(std::vector<PathNode*> destinations)
{
	if (GetPathDestination() != nullptr)
		return;

	GameLoop& game = GameLoop::Instance();
	Pathfinder* pathfinder = game.pathfinder;
	PathNode* currNode = game.GetGrid().GetNodeAt(position);
	std::vector<PathNode*> path;
	int shortestDist = INT_MAX;

	for (PathNode* dest : destinations)
	{
		float pathDist = 0;
		std::vector<PathNode*> currPath = pathfinder->RequestPath(currNode, dest, pathDist, radius);
		if (pathDist < shortestDist)
		{
			shortestDist = pathDist;
			path = currPath;
		}
	}

	if (path.empty())
		return;

	SetState(State::STATE_FOLLOW_PATH);
	behaviour->SetPath(path);
}

PathNode* GameAI::GetPathDestination()
{
	 return behaviour->GetDestinationNode(); 
}
