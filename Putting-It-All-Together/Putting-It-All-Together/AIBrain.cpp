#include "AIBrain.h"
#include "AIBrainManagers.h"
#include "GameLoop.h"
#include "Logger.h"
#include <algorithm>

AIBrain::AIBrain(GameAI* owner) : ownerAI(owner)
{
	resources = std::make_unique<ResourceManager>(this);
	transport = std::make_unique<TransportManager>(this);
	build = std::make_unique<BuildManager>(this);
	manufacturing = std::make_unique<ManufacturingManager>(this);
	military = std::make_unique<MilitaryManager>(this);
	allocator = std::make_unique<TaskAllocator>(this);

	// initialize some inventory
	resources->Add(ResourceType::Wood, 0);
	resources->Add(ResourceType::Coal, 0);
	resources->Add(ResourceType::Iron, 0);

	// Desire: 20 soldiers
	AddDesire("HaveSoldiers", TaskType::TrainSoldiers, ResourceType::None, 20, 1.0f);

	Grid& grid = GameLoop::Instance().GetGrid();
	int rows = grid.GetRows();
	int cols = grid.GetCols();
	knownNodes.assign(rows, std::vector<KnownNode>(cols));
}

AIBrain::~AIBrain()
{
	ownerAI = nullptr;
}

void AIBrain::Think(float deltaTime)
{
	if (!ownerAI)
		return;

	Decay(deltaTime);
	UpdateValues(deltaTime);

	// update managers
	UpdateDiscovered();

	resources->Update(deltaTime);
	transport->Update(deltaTime);
	build->Update(deltaTime);
	manufacturing->Update(deltaTime);
	military->Update(deltaTime);
	allocator->Update(deltaTime);

	FSM(deltaTime);
	CheckDeath();
}

void AIBrain::SetMaterialPriority(float p) { materialPriority = p; }
void AIBrain::SetLaborPriority(float p) { laborPriority = p; }
void AIBrain::SetConstructionPriority(float p) { constructionPriority = p; }

void AIBrain::AddDesire(const std::string& name, TaskType taskType, ResourceType primaryResource, int targetCount, float importance)
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

	Grid& grid = GameLoop::Instance().GetGrid();
	double gameTime = GameLoop::Instance().GetGameTime();

	std::vector<PathNode*> visible;
	float visionRadius = ownerAI->GetRadius() * 5;

	grid.QueryNodes(ownerAI->GetPosition(), visionRadius, visible);

	for (PathNode* node : visible)
	{
		if (!grid.HasLineOfSight(ownerAI->GetPosition(), node->position, 1) && !node->IsObstacle())
			continue;
		
		int r, c;
		grid.WorldToGrid(node->position, r, c);

		knownNodes[r][c].discovered = true;
		knownNodes[r][c].walkable = !node->IsObstacle();
		knownNodes[r][c].lastSeenTime = gameTime;
		knownNodes[r][c].resource = NodeToResource(node->type);
	}
}

void AIBrain::UpdateValues(float deltaTime)
{
	// Desire-driven plan for training soldiers
	for (Desire& d : desires)
	{
		if (d.added)
			continue;

		d.added = true;

		if (d.fulfillTaskType == TaskType::TrainSoldiers)
		{
			int amount = d.targetCount;
			float basePrio = d.importance;

			Task t;
			t.type = TaskType::TrainSoldiers;
			t.priority = basePrio;
			t.amount = amount;
			allocator->AddTask(t);
		}
	}
}

void AIBrain::Decay(float deltaTime)
{
	materialPriority = std::max(0.1f, materialPriority - deltaTime * 0.01f);
	laborPriority = std::max(0.1f, laborPriority - deltaTime * 0.005f);
	constructionPriority = std::max(0.1f, constructionPriority - deltaTime * 0.007f);
}

void AIBrain::GatherResource(ResourceType resourceType, Task& t, float deltaTime)
{

	if (resources->Get(resourceType) > t.amount)
	{
		allocator->RemoveTask(t.id);
	}

	Grid& grid = GameLoop::Instance().GetGrid();
	std::vector<PathNode*> nodes;

	grid.QueryNodes(ownerAI->GetPosition(), ownerAI->GetRadius(), nodes, ResourceToNode(resourceType));
	if (!nodes.empty())
	{
		// is close
		resources->Add(resourceType, 60 * deltaTime);
	}
	else
	{
		bool valid = true;
		// is not close, go to closest
		ownerAI->GoToClosest(ResourceToNode(resourceType), valid);

		if (!valid)
		{
			Task tt;
			tt.type = TaskType::Discover;
			tt.priority = t.priority + 1;
			tt.resource = resourceType;
			allocator->AddTask(tt);
		}
	}
}

void AIBrain::FSM(float deltaTime)
{
	if (!ownerAI) 
		return;

	if (!allocator->HasPending() && ownerAI->GetCurrentState() != GameAI::State::STATE_IDLE)
	{
		ownerAI->SetState(GameAI::State::STATE_IDLE);
		return;
	}

	Grid& grid = GameLoop::Instance().GetGrid();
	std::vector<PathNode*> nodes;

	Vec2 barrackLoc;
	Task gatherWood;
	Task gatherIron;
	Task bTask;
	Vec2 baseBarrackLoc = Vec2(990.000000, 670.000000);
	PathNode* frontier;

	Task* tt = allocator->GetNext();
	if (tt == nullptr)
		return;
	Task& t = *tt;
	if (t.id != prevTaskId)
		Logger::Instance().Log(ownerAI->GetName() + " assigned to " + ToString(t.type) + "(task id : " + std::to_string(t.id) + " priority = " + std::to_string(t.priority) + ")\n");


	switch (t.type)
	{
	case TaskType::FellTrees:

		GatherResource(ResourceType::Wood, t, deltaTime);
		break;
	case TaskType::MineCoal:

		GatherResource(ResourceType::Coal, t, deltaTime);
		break;
	case TaskType::MineIron:

		GatherResource(ResourceType::Iron, t, deltaTime);
		break;
	case TaskType::Transport:
		ownerAI->SetState(GameAI::State::STATE_FOLLOW_PATH);
		break;
	case TaskType::TrainSoldiers:
		if (t.amount <= 0)
		{
			allocator->RemoveTask(t.id);
			break;
		}

		if (build->IsInQueue("Barrack"))
		{
			Logger::Instance().Log(ownerAI->GetName() + " waiting for barrack to train soldiers \n");
			break;
		}

		if (!build->HasBuilding("Barrack"))
		{
			Logger::Instance().Log(ownerAI->GetName() + " no barrack to train soldiers \n");
			bTask.type = TaskType::Build;
			bTask.meta = "Barrack";
			bTask.priority = t.priority + 1;
			allocator->AddTask(bTask);
			break;
		}

		barrackLoc = build->GetBuildingLocation("Barrack");

		if (DistanceBetween(ownerAI->GetPosition(), barrackLoc) > ownerAI->GetRadius() + 2)
		{
			PathNode* barrackNode = grid.GetNodeAt(barrackLoc + Vec2(0, 1));

			bool valid = true;
			ownerAI->GoTo(barrackNode, valid);

			// if not valid, explore
			Task tt;
			tt.type = TaskType::Discover;
			tt.priority = t.priority + 1;
			tt.destination = baseBarrackLoc;
			allocator->AddTask(tt);
		}
		else
		{
			if (resources->Request(ResourceType::Iron, COST_IRON_PER_SOLDIER) && resources->Request(ResourceType::Wood, COST_WOOD_PER_SOLDIER))
			{
				military->TrainSoldiers(1);
				t.amount--;
			}
			else
			{
				Logger::Instance().Log(ownerAI->GetName() + " not enough resources to train soldier \n");

				int prio = t.priority + 1;

				gatherWood.type = TaskType::FellTrees;
				gatherWood.resource = ResourceType::Wood;
				gatherWood.priority = prio;
				gatherWood.amount = COST_WOOD_PER_SOLDIER * t.amount;
				allocator->AddTask(gatherWood);

				gatherIron.type = TaskType::MineIron;
				gatherIron.resource = ResourceType::Iron;
				gatherIron.priority = prio;
				gatherIron.amount = COST_IRON_PER_SOLDIER * t.amount;
				allocator->AddTask(gatherIron);
			}

		}
		break;
	case TaskType::Build:

		if (build->HasBuilding(t.meta) || build->IsInQueue(t.meta))
			allocator->RemoveTask(t.id);

		if (resources->Get(ResourceType::Wood) < BARRACK_WOOD_COST)
		{
			gatherWood.type = TaskType::FellTrees;
			gatherWood.resource = ResourceType::Wood;
			gatherWood.priority = t.priority + 1;
			gatherWood.amount = BARRACK_WOOD_COST;
			allocator->AddTask(gatherWood);
			Logger::Instance().Log(ownerAI->GetName() + " cannot afford barrack \n");
			break;
		}
		if (resources->Get(ResourceType::Iron) < BARRACK_IRON_COST)
		{
			gatherIron.type = TaskType::MineIron;
			gatherIron.resource = ResourceType::Iron;
			gatherIron.priority = t.priority + 1;
			gatherIron.amount = BARRACK_IRON_COST;
			allocator->AddTask(gatherIron);
			Logger::Instance().Log(ownerAI->GetName() + " cannot afford barrack \n");
			break;
		}

		if (DistanceBetween(ownerAI->GetPosition(), baseBarrackLoc) > ownerAI->GetRadius() + 2)
		{

			bool valid = true;
			ownerAI->GoTo(grid.GetNodeAt(baseBarrackLoc), valid);

			// if not valid, explore
			Task tt;
			tt.type = TaskType::Discover;
			tt.priority = t.priority + 1;
			tt.destination = baseBarrackLoc;
			allocator->AddTask(tt);

			break;
		}

		if (resources->Request(ResourceType::Iron, BARRACK_IRON_COST) && resources->Request(ResourceType::Wood, BARRACK_WOOD_COST))
		{
			build->QueueBuilding(t.meta, baseBarrackLoc);
			allocator->RemoveTask(t.id);
			break;
		}

		break;
	case TaskType::Discover:
		for (int row = 0; row < grid.GetRows(); ++row)
		{
			for (int col = 0; col < grid.GetCols(); ++col)
			{

				auto& k = knownNodes[row][col];

				if (k.discovered && (k.resource == t.resource || t.resource == ResourceType::None))
				{
					Logger::Instance().Log(ownerAI->GetName() + " discovered resource \n");
					allocator->RemoveTask(t.id);
					break;
				}
			}
		}

		frontier = FindClosestFrontier();
		if (frontier)
		{
			bool valid = true;
			ownerAI->GoTo(frontier, valid);
		}

		break;
	default: break;
	}

	prevTaskId = t.id;
}

void AIBrain::CheckDeath()
{
	bool someCondition = false;
	// placeholder: no death logic yet
	if (someCondition)
	{
		Logger::Instance().Log(ownerAI->GetName() + " has died due to unmet needs.\n");
		ownerAI->SetState(GameAI::State::STATE_IDLE);
		GameLoop::Instance().ScheduleDeath(ownerAI);
	}
}

bool AIBrain::IsFrontierNode(const PathNode* node)
{
	Grid& grid = GameLoop::Instance().GetGrid();

	int r = -1;
	int c = -1;
	grid.WorldToGrid(node->position, r, c);

	const KnownNode& k = knownNodes[r][c];

	if (!k.discovered)
		return false;

	if (node->clearance < ownerAI->GetRadius())
		return false;

	for (PathNode* n : node->neighbors)
	{
		int nr = -1;
		int nc = -1;
		grid.WorldToGrid(n->position, nr, nc);
		const KnownNode& kn = knownNodes[nr][nc];

		if (!kn.discovered)
			return true;
	}
	return false;
}

PathNode* AIBrain::FindClosestFrontier()
{
	GameLoop& game = GameLoop::Instance();
	Grid& grid = game.GetGrid();

	PathNode* start = grid.GetNodeAt(ownerAI->GetPosition());

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

		if (IsFrontierNode(current))
			return current;

		for (PathNode* n : current->neighbors)
		{
			int r = -1;
			int c = -1;
			grid.WorldToGrid(n->position, r, c);

			if (!visited.contains(n) && knownNodes[r][c].discovered && knownNodes[r][c].walkable)
			{
				visited.insert(n);
				q.push(n);
			}
		}
	}

	return nullptr;
}
