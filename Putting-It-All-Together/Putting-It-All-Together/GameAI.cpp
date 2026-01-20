#include <iostream>
#include "GameAI.h"
#include "GameLoop.h"
#include "Behaviour.h"
#include "AIBrain.h"
#include "PathNode.h"


GameAI::GameAI(Vec2 pos) :
	currentState(State::STATE_IDLE)
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
}

void GameAI::SetState(State state)
{
	if (state != currentState)
		Logger::Instance().Log("State changed to: " + ToString(state) + "\n");
	currentState = state;
}

void GameAI::Update(float deltaTime)
{
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

	Movable::Update(deltaTime);
}

bool GameAI::CanUseNode(const PathNode* node) const
{
	if (!connectedBrain)
		return !node->IsObstacle();

	Grid& grid = GameLoop::Instance().GetGrid();

	int r, c;
	grid.WorldToGrid(node->position, r, c);

	KnownNode* k = &connectedBrain->knownNodes[r][c];

	// Option A: unknown = blocked
	if (!k->discovered)
		return false;

	return k->walkable;
}

bool GameAI::CanGoTo(PathNode* destination, float& dist)
{
	if (!destination)
		return false;

	GameLoop& game = GameLoop::Instance();
	Pathfinder* pathfinder = game.pathfinder;
	PathNode* currNode = game.GetGrid().GetNodeAt(position);
	float pathDist = 0;
	std::vector<PathNode*> path;

	auto filter = [this](const PathNode* node) { return CanUseNode(node); };

	path = pathfinder->RequestPath(currNode, destination, pathDist, radius, filter);
	dist = pathDist;

	if (path.empty())
	{
		return false;
	}

	return true;
}

void GameAI::GoTo(PathNode* destination, bool &isPathValid, bool ignoreFog)
{
	if (!destination)
		return;

	// Update intent
	desiredDestination = destination;

	GameLoop& game = GameLoop::Instance();
	Pathfinder* pathfinder = game.pathfinder;
	PathNode* currNode = game.GetGrid().GetNodeAt(position);
	float pathDist = 0;
	std::vector<PathNode*> path;

	auto filter = [this](const PathNode* node) { return CanUseNode(node); };
	auto filter2 = [this](const PathNode* node) { return !node->IsObstacle(); };

	if (!ignoreFog)
		path = pathfinder->RequestPath(currNode, destination, pathDist, radius, filter);
	else
		path = pathfinder->RequestPath(currNode, destination, pathDist, radius, filter2);

	if (path.empty())
	{
		isPathValid = false;
		return;
	}

	SetState(State::STATE_FOLLOW_PATH);
	behaviour->SetPath(path);
	isPathValid = true;
}

void GameAI::GoToClosest(PathNode::ResourceType destinationType, bool& isPathValid)
{
	GoToClosest(std::vector<PathNode::ResourceType>(destinationType), isPathValid);
}
void GameAI::GoToClosest(std::vector<PathNode::ResourceType> destinationTypes, bool& isPathValid)
{
	if (destinationTypes.empty())
		return;

	GameLoop& game = GameLoop::Instance();
	Pathfinder* pathfinder = game.pathfinder;
	PathNode* currNode = game.GetGrid().GetNodeAt(position);
	std::vector<PathNode*> path;
	float dist = 0;

	path = pathfinder->RequestClosestPath(currNode, destinationTypes, dist, radius, [this](const PathNode* node) { return CanUseNode(node); });

	if (path.empty())
	{
		isPathValid = false;
		return;
	}

	SetState(State::STATE_FOLLOW_PATH);
	behaviour->SetPath(path);
	isPathValid = true;
}

PathNode* GameAI::GetPathDestination()
{
	 return behaviour->GetDestinationNode(); 
}
