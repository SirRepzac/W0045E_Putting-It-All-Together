#include "AIBrain.h"
#include "AIBrainManagers.h"
#include "GameLoop.h"
#include "Logger.h"
#include <algorithm>
#include "random.h"

AIBrain::AIBrain()
{
	resources = std::make_unique<ResourceManager>(this);
	build = std::make_unique<BuildManager>(this);
	manufacturing = std::make_unique<ManufacturingManager>(this);
	population = std::make_unique<PopulationManager>(this);
	taskAllocator = std::make_unique<TaskAllocator>(this);

	// initialize some inventory
	resources->Add(ItemType::Wood, 0);
	resources->Add(ItemType::Coal, 0);
	resources->Add(ItemType::Iron, 0);

	// Desire: 20 soldiers
	//AddDesire("HaveSoldiers", TaskType::TrainUnit, ItemType::None, 20, 1.0f);

	Grid& grid = GameLoop::Instance().GetGrid();
	int rows = grid.GetRows();
	int cols = grid.GetCols();
	knownNodes.assign(rows, std::vector<KnownNode>(cols));

	Vec2 startingPos = startPos;
	PathNode* startNode = grid.GetNodeAt(startingPos);
	homeNode = startNode;
	KnownNode& kNode = NodeToKnown(homeNode);
	kNode.resource = PathNode::ResourceType::Building;

	double gameTime = GameLoop::Instance().GetGameTime();
	ExploreNode(startNode, grid, gameTime);
	for (auto n : startNode->neighbors)
	{
		ExploreNode(n, grid, gameTime);
	}

	std::vector<GameAI*> workers = GameLoop::Instance().CreateAI(50, startingPos);
	for (int i = 0; i < workers.size(); i++)
	{
		GameAI* ai = workers[i];

		RNG rng(SeedFromNode(i));

		// Random in [-1, 1]
		float u = rng.NextFloat01() * 2.0f - 1.0f;
		float v = rng.NextFloat01() * 2.0f - 1.0f;

		float radius = ai->GetRadius();

		// 3x3 nodes => half extent = 1.5 cells
		float halfExtent = 1.5f * grid.cellSize - radius;

		float offsetX = u * halfExtent;
		float offsetY = v * halfExtent;

		ai->SetPos(startingPos + Vec2(offsetX, offsetY));
		Agent* worker = new Agent(ai);
		worker->brain = this;
		worker->ai->ConnectBrain(this);
		agents.push_back(worker);
		populationMap[PopulationType::Worker].push_back(worker);
	}

	TrainUnit(PopulationType::Scout);
	TrainUnit(PopulationType::Scout);
	TrainUnit(PopulationType::Scout);
	TrainUnit(PopulationType::Scout);

	TrainUnit(PopulationType::Smelter);
	TrainUnit(PopulationType::Coal_Miner);
	TrainUnit(PopulationType::ArmSmith);
	TrainUnit(PopulationType::Builder);

	BuildBuilding(BuildingType::Smelter);
	BuildBuilding(BuildingType::Coal_Mine);
	BuildBuilding(BuildingType::Forge);
	BuildBuilding(BuildingType::Training_Camp);

}

AIBrain::~AIBrain()
{
	for (auto a : agents)
		delete a;
}

void AIBrain::Think(float deltaTime)
{
	// update managers
	resources->Update(deltaTime);
	build->Update(deltaTime);
	manufacturing->Update(deltaTime);
	population->Update(deltaTime);
	taskAllocator->Update(deltaTime);

	FSM(deltaTime);
	CheckDeath();
}

void AIBrain::FSM(float dt)
{
	frames++;

	UpdatePopulationTasks(dt);

	int i = frames % agents.size();

	int updated = 0;
	std::string s;
	int agentsPerFrame = 10;
	for (int j = 0; j < agentsPerFrame; j++)
	{
		int idx = (i + j * agents.size() / agentsPerFrame) % agents.size();
		s += std::to_string(idx) + " ,";
		agents[idx]->Update(dt);
		updated++;
	}

	UpdateDiscovered();
	PickupNewTrained();

	//if (needs building b)
	//{
	//	PathNode* fittingNode = GetBuildingLocation(b);
	//	Building* toBuild = build->QueueBuilding(building.buildingType, fittingNode);
	//	Task t;
	//	t.type = TaskType::Build;
	//	t.buildingType = b;
	//	t.priority = somepriority;
	//	t.building = toBuild;
	//	taskAllocator->AddTask(t);
	//}

	//if (needs wood for b)
	//{
	//	Task t;
	//	t.type = TaskType::GatherWood;
	//	t.amount = 1;
	//	t.buildingType = b.buildingType;
	//	t.building = b;
	//	t.priority = b.priority;
	//	taskAllocator->AddTask(t);
	//}
}

void AIBrain::BuildBuilding(BuildingType b, PathNode* node)
{
	if (node == nullptr)
	{
		node = GetBuildingLocation(b);
	}

	Building* toBuild = build->QueueBuilding(b, node);


	KnownNode& kNode = NodeToKnown(node);

	kNode.resource = PathNode::ResourceType::Building;

	Building* te = build->GetBuildingTemplate(b);

	GatherWood(te->cost.resources[ItemType::Wood], 1.0f, toBuild);
}

void AIBrain::GatherWood(int amount, float priority, Building* building)
{
	Task t;
	t.type = TaskType::GatherWood;
	t.amount = amount;
	t.building = building;
	t.priority = priority;
	taskAllocator->AddTask(t);
}

void AIBrain::AddDesire(const std::string& name, TaskType taskType, ItemType primaryResource, int targetCount, float importance)
{
	Desire d;
	d.name = name;
	d.fulfillTaskType = taskType;
	d.targetCount = targetCount;
	d.importance = importance;
	desires.push_back(d);
}

void AIBrain::UpdateDiscovered()
{
	if (discoveredAll)
		return;

	Grid& grid = GameLoop::Instance().GetGrid();
	double gameTime = GameLoop::Instance().GetGameTime();

	std::vector<PathNode*> visible;

	for (auto scout : populationMap[PopulationType::Scout])
	{
		PathNode* currentNode = grid.GetNodeAt(scout->ai->GetPosition());
		if (currentNode->IsObstacle())
			continue;
		visible = currentNode->neighbors;

		for (PathNode* node : visible)
		{
			ExploreNode(node, grid, gameTime);
		}
	}
}

void AIBrain::ExploreNode(PathNode* node, Grid& grid, double& gameTime)
{
	if (!node)
		return;

	int r, c;
	grid.WorldToGrid(node->position, r, c);

	KnownNode& kNode = knownNodes[r][c];

	kNode.walkable = !node->IsObstacle();
	kNode.lastSeenTime = gameTime;

	if (kNode.discovered)
		return;

	kNode.resource = node->resource;
	kNode.resourceAmount = node->resourceAmount;
	kNode.discovered = true;

	if (node->resource != PathNode::ResourceType::None && node->resourceAmount > 0)
	{
		knownResources[node->resource].push_back(node);
	}

	GameLoop::Instance().renderer->MarkNodeDirty(grid.Index(c, r));
}

bool AIBrain::IsDiscovered(int index) const 
{
	Grid& grid = GameLoop::Instance().GetGrid();
	auto indexPair = grid.TwoDIndex(index);

	return knownNodes[indexPair.first][indexPair.second].discovered;
}

bool AIBrain::IsDiscovered(int row, int col) const 
{
	return knownNodes[row][col].discovered;
}

bool AIBrain::IsDiscovered(const PathNode* node)
{
	if (!node)
		return false;

	KnownNode& kNode = NodeToKnown(node);

	return kNode.discovered;
}

// not used
Agent* AIBrain::GetBestAgent(PopulationType type, PathNode* node)
{
	float bestScore = FLT_MAX;
	Agent* bestAgent = nullptr;
	for (Agent* agent : populationMap[type])
	{
		if (agent->busy)
			continue;

		float dist;
		if (agent->ai->CanGoTo(node, dist))
		{
			if (dist < bestScore)
			{
				bestScore = dist;
				bestAgent = agent;
			}
		}
	}
	return bestAgent;
}

void AIBrain::UpdatePopulationTasks(float dt)
{
	for (Agent* agent : populationMap[PopulationType::Worker])
	{
		if (agent->busy)
		{
			continue;
		}

		Task* t = nullptr;
		if (agent->holding == ItemType::None)
			t = taskAllocator->GetNext(TaskType::GatherWood);

		if (t)
		{
			agent->currentTask = t;
			agent->busy = true;
		}
	}
	for (Agent* agent : populationMap[PopulationType::ArmSmith])
	{
		if (agent->busy)
		{
			continue;
		}

		Task* t = taskAllocator->GetNext(TaskType::ForgeWeapon);

		if (t)
		{
			agent->currentTask = t;
			agent->busy = true;
		}

	}
	for (Agent* agent : populationMap[PopulationType::Builder])
	{
		if (agent->busy)
		{
			continue;
		}

		Task* t = taskAllocator->GetNext(TaskType::Build);

		if (t)
		{
			agent->currentTask = t;
			agent->busy = true;
		}

	}
	for (Agent* agent : populationMap[PopulationType::Coal_Miner])
	{
		if (agent->busy)
		{
			continue;
		}

		Task* t = taskAllocator->GetNext(TaskType::MineCoal);

		if (t)
		{
			agent->currentTask = t;
			agent->busy = true;
		}

	}
	for (Agent* agent : populationMap[PopulationType::Smelter])
	{
		if (agent->busy)
		{
			continue;
		}

		Task* t = taskAllocator->GetNext(TaskType::Smelt);

		if (t)
		{
			agent->currentTask = t;
			agent->busy = true;
		}

	}
	for (Agent* agent : populationMap[PopulationType::Scout])
	{
		if (agent->busy)
		{
			continue;
		}

		Task t;
		t.type = TaskType::Explore;
		t.priority = 1.0f;
		taskAllocator->AddTask(t);

		Task* tptr = taskAllocator->GetNext(TaskType::Explore);

		if (tptr)
		{
			agent->currentTask = tptr;
			agent->busy = true;
		}
	}
}

void AIBrain::TrainUnit(PopulationType type)
{
	Agent* agent = nullptr;
	std::vector<Agent*>::iterator it;
	for (it = populationMap[PopulationType::Worker].begin(); it < populationMap[PopulationType::Worker].end();)
	{
		if (!((*it)->busy))
		{
			agent = *it;
			break;
		}
		else
			it++;
	}

	if (it == populationMap[PopulationType::Worker].end())
		return;

	auto templat = population->GetTemplate(type);

	std::vector<std::pair<ItemType, float>> lackingResources;

	if (templat->CanAfford(resources->inventory, lackingResources))
	{
		templat->RemoveResources(resources->inventory);
		population->TrainUnit(type, agent);
		populationMap[PopulationType::Worker].erase(it);
	}
}

void AIBrain::PickupNewTrained()
{
	for (Agent* agent : population->finishedUnits)
	{
		populationMap[agent->type].push_back(agent);
	}

	population->finishedUnits.clear();
}

void AIBrain::CheckDeath()
{
	GameAI* ownerAI = nullptr;
	return;

	bool someCondition = false;
	// placeholder: no death logic yet
	if (someCondition)
	{
		Logger::Instance().Log(ownerAI->GetName() + " has died.\n");
		ownerAI->SetState(GameAI::State::STATE_IDLE, "death");
		GameLoop::Instance().ScheduleDeath(ownerAI);
	}
}

static PathNode* BFS(PathNode* startNode, std::vector<std::vector<KnownNode>>& knownNodes, std::function<bool(const PathNode*)> filter)
{
	GameLoop& game = GameLoop::Instance();
	Grid& grid = game.GetGrid();

	PathNode* start = startNode;

	int sr, sc;
	grid.WorldToGrid(start->position, sr, sc);

	if (!knownNodes[sr][sc].discovered)
	{
		// ensure at least current node is known
		knownNodes[sr][sc].discovered = true;
		knownNodes[sr][sc].walkable = !start->IsObstacle();
	}

	std::queue<PathNode*> q;
	std::unordered_set<PathNode*> visited;

	q.push(start);
	visited.insert(start);

	while (!q.empty())
	{
		PathNode* current = q.front();
		q.pop();

		if (filter(current))
		{
			return current;
		}

		for (PathNode* n : current->neighbors)
		{
			int r = -1;
			int c = -1;
			grid.WorldToGrid(n->position, r, c);

			if (visited.find(n) == visited.end())
			{
				visited.insert(n);
				q.push(n);
			}
		}
	}
}

// most often return top left frontier node
PathNode* AIBrain::FindClosestFrontier(Agent* agent)
{
	auto filter = [this](const PathNode* node) { return !IsDiscovered(node) && !node->IsObstacle(); };

	GameLoop& game = GameLoop::Instance();
	Grid& grid = game.GetGrid();

	PathNode* start = grid.GetNodeAt(agent->ai->GetPosition());

	return BFS(start, knownNodes, filter);
}

PathNode* AIBrain::GetBuildingLocation(BuildingType type)
{
	if (buildingLoc.find(type) != buildingLoc.end())
		return buildingLoc.at(type);

	Building* b = build->GetBuildingTemplate(type);

	auto filter = [this](const PathNode* node) 
		{ 
			return NodeToKnown(node).resource == PathNode::ResourceType::None;
		};

	GameLoop& game = GameLoop::Instance();
	Grid& grid = game.GetGrid();

	PathNode* buildNode = BFS(homeNode, knownNodes, filter);

	if (buildNode == nullptr)
		Logger::Instance().Log("Buildnode set to null for " + ToString(type) + "\n");

	buildingLoc[type] = buildNode;
	return buildNode;
}

void Agent::Update(float dt)
{

	if (currentTask == nullptr)
		return;

	if (type == PopulationType::Worker)
	{
		if (currentTask->type == TaskType::GatherWood)
		{
			if (brain->knownResources[PathNode::ResourceType::Wood].size() == 0)
				return;

			GameLoop& game = GameLoop::Instance();
			Grid& grid = game.GetGrid();
			std::vector<PathNode*> nodes;
			grid.QueryNodes(ai->GetPosition(), ai->GetRadius() * 2, nodes, PathNode::ResourceType::Wood);

			// is close to resource
			for (std::vector<PathNode*>::iterator it = nodes.begin(); it != nodes.end();)
			{
				if ((*it)->resourceAmount <= 0)
					it = nodes.erase(it);
				else
					it++;
			}

			if (!nodes.empty())
			{
				if (workTimer >= 30.0f)
				{
					PathNode* node = nodes[0];
					holding = ItemType::Wood;

					node->resourceAmount--;
					if (node->resourceAmount <= 0)
					{
						node->resource = PathNode::ResourceType::None;
						int r, c;
						grid.WorldToGrid(node->position, r, c);
						GameLoop::Instance().renderer->MarkNodeDirty(grid.Index(c, r));

					}
					bool valid = true;
					ai->GoTo(currentTask->building->targetNode, valid);
					currentTask->type = TaskType::Transport;
					//busy = false;
				}
				else
				{
					workTimer += dt;
				}
			}
			else
			{
				if (approaching)
				{
					bool valid = true;
					ai->GoTo(approaching, valid);
					return;
				}

				Pathfinder* p = game.pathfinder;

				PathNode* currentNode = grid.GetNodeAt(ai->GetPosition());

				float outDist;

				auto filter = [this](const PathNode* node) { return brain->CanUseNode(node); };

				std::vector<PathNode*> path = p->RequestClosestPath(currentNode, brain->KnownNodesOfType(PathNode::ResourceType::Wood), outDist, ai->GetRadius(), filter);
				if (path.empty())
				{
					return;
				}

				PathNode* closest = path.front();

				KnownNode& kNode = brain->NodeToKnown(closest);

				kNode.resourceAmount--;
				approaching = closest;

				bool valid = true;
				ai->GoTo(closest, valid);
			}
		}
		else if (currentTask->type == TaskType::Transport)
		{
			if (DistanceBetween(ai->GetPosition(), currentTask->building->targetNode->position) < ai->GetRadius() * 2)
			{
				Logger::Instance().Log("delivered " + ToString(holding) + "\n");

				if (currentTask->building->AddResource(holding))
				{
					holding = ItemType::None;
				}

				currentTask->completed = true;
				busy = false;
				currentTask = nullptr;

				bool valid = true;

				ai->GoTo(brain->homeNode, valid);
			}
			else
			{
				bool valid = true;
				ai->GoTo(currentTask->building->targetNode, valid);
			}
		}
	}

	if (type == PopulationType::Scout)
	{
		if (brain->discoveredAll)
			return;

		if (!ai->GetPathDestination() || brain->IsDiscovered(ai->GetPathDestination()))
		{
			PathNode* node = brain->FindClosestFrontier(this);
			if (!node)
			{
				brain->discoveredAll = true;
				ai->SetState(GameAI::State::STATE_IDLE, "found all nodes");
				return;
			}
			bool valid = true;
			ai->GoTo(node, valid, true);
		}
	}

	if (type == PopulationType::Soldier)
	{

	}

	if (type == PopulationType::ArmSmith)
	{

	}

	if (type == PopulationType::Coal_Miner)
	{

	}

	if (type == PopulationType::Smelter)
	{

	}

	if (type == PopulationType::Builder)
	{
		if (currentTask->building == nullptr)
			return;
		if (currentTask->building->built)
		{
			currentTask->completed = true;
			busy = false;
			return;
		}
		if (DistanceBetween(ai->GetPosition(), currentTask->building->targetNode->position) < ai->GetRadius() * 2)
		{
			currentTask->building->WorkOnBuilding(dt);
			if (currentTask->building->productionTime <= 0)
			{
				currentTask->completed = true;
				currentTask = nullptr;
				busy = false;
				return;
			}
		}
		else
		{
			if (currentTask->building->HasCost())
				return;

			bool valid = true;
			ai->GoTo(currentTask->building->targetNode, valid);

		}
	}
}

std::vector<PathNode*> AIBrain::KnownNodesOfType(PathNode::ResourceType type)
{
	Grid& grid = GameLoop::Instance().GetGrid();

	std::vector<PathNode*> known;
	for (auto node : knownResources[type])
	{
		int r;
		int c;
		grid.WorldToGrid(node->position, r, c);

		if (knownNodes[r][c].resourceAmount > 0)
			known.push_back(node);
	}

	return known;
}

bool AIBrain::CanUseNode(const PathNode* node)
{
	Grid& grid = GameLoop::Instance().GetGrid();

	int r, c;
	grid.WorldToGrid(node->position, r, c);

	KnownNode& k = knownNodes[r][c];

	if (!k.discovered)
		return false;

	return k.walkable;
}

KnownNode& AIBrain::NodeToKnown(const PathNode* node) 
{
	Grid& grid = GameLoop::Instance().GetGrid();

	int r, c;
	grid.WorldToGrid(node->position, r, c);

	return knownNodes[r][c];
}