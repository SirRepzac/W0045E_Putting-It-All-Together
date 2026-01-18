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
	resources->Add(ItemType::Wood, 0);
	resources->Add(ItemType::Coal, 0);
	resources->Add(ItemType::Iron, 0);

	// Desire: 20 soldiers
	AddDesire("HaveSoldiers", TaskType::TrainSoldiers, ItemType::None, 20, 1.0f);

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

	Grid& grid = GameLoop::Instance().GetGrid();
	double gameTime = GameLoop::Instance().GetGameTime();

	std::vector<PathNode*> visible;
	float visionRadius = ownerAI->GetRadius() * 5;

	if (grid.GetNodeAt(ownerAI->GetPosition())->IsObstacle())
		return;

	grid.QueryNodes(ownerAI->GetPosition(), visionRadius, visible);

	for (PathNode* node : visible)
	{
		if (!grid.HasLineOfSight(ownerAI->GetPosition(), node->position, 1) && !node->IsObstacle())
			continue;

		Discover(node, grid, gameTime);
	}
}

void AIBrain::Discover(PathNode* node, Grid& grid, double& gameTime)
{
	int r, c;
	grid.WorldToGrid(node->position, r, c);

	KnownNode& kNode = knownNodes[r][c];

	kNode.walkable = !node->IsObstacle();
	kNode.lastSeenTime = gameTime;

	if (kNode.discovered)
		return;

	kNode.discovered = true;

	if (node->resource != PathNode::ResourceType::None && node->resourceAmount > 0)
	{
		knownResources[ResourceToItem(node->resource)].push_back(node);
	}

	GameLoop::Instance().renderer->MarkNodeDirty(grid.Index(c, r));
}

bool AIBrain::IsDiscovered(int index) const
{
	Grid& grid = GameLoop::Instance().GetGrid();
	auto indexPair = grid.TwoDIndex(index);

	return knownNodes[indexPair.first][indexPair.second].discovered;
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

void AIBrain::GatherResources(Task& t, float deltaTime)
{
	std::vector<std::pair<ItemType, float>>& resourcePairs = t.resources;
	std::vector<std::pair<ItemType, float>> res;
	bool endTask = true;
	for (std::pair<ItemType, float> p : resourcePairs)
	{
		if (resources->Get(p.first) > p.second)
			continue;
		res.push_back(p);
		endTask = false;
	}
	if (endTask)
	{
		std::string ss;
		for (auto l : t.resources)
		{
			ss += std::to_string(resources->Get(l.first)) + "/" + std::to_string(l.second) + " " + ToString(l.first) + ", ";
		}
		Logger::Instance().Log(ownerAI->GetName() + " quit gathering with: " + ss + "\n");

		allocator->RemoveTask(t.id);
		return;
	}

	std::vector<PathNode::ResourceType> types;
	for (auto p : res)
	{
		types.push_back(ItemToResource(p.first));
	}

	Grid& grid = GameLoop::Instance().GetGrid();
	std::vector<PathNode*> nodes;
	std::vector<PathNode*> healthynodes;

	grid.QueryNodes(ownerAI->GetPosition(), ownerAI->GetRadius() * 2, nodes, types);

	for (PathNode* node : nodes)
	{
		if (node->resourceAmount > 0)
			healthynodes.push_back(node);
	}

	if (!healthynodes.empty())
	{
		PathNode* healthyNode = healthynodes[0];
		// is close
		healthyNode->resourceAmount -= 20 * deltaTime;
		ItemType itemType = ResourceToItem(healthyNode->resource);
		resources->Add(itemType, 20 * deltaTime);

		if (healthyNode->resourceAmount <= 0)
		{
			int r;
			int c;
			grid.WorldToGrid(healthyNode->position, r, c);
			GameLoop::Instance().renderer->MarkNodeDirty(grid.Index(c, r));

			std::vector<PathNode*>& knownList = knownResources[itemType];

			for (std::vector<PathNode*>::iterator it = knownList.begin(); it != knownList.end();)
			{
				if (*it == healthyNode)
				{
					it = knownList.erase(it);
					break;
				}
				else
					++it;
			}
		}
	}
	else
	{
		bool valid = true;
		// is not close, go to closest
		ownerAI->GoToClosest(types, valid);

		if (!valid)
		{
			Task tt;
			tt.type = TaskType::Discover;
			tt.priority = t.priority + 1;
			tt.time = 5.0f;
			tt.resources = res;
			allocator->AddTask(tt);
		}
	}
}

void AIBrain::ManufactureProducts(Task& t, float deltaTime)
{
	std::vector<std::pair<ItemType, float>>& resourcePairs = t.resources;
	std::vector<std::pair<ItemType, float>> res;
	bool endTask = true;
	for (std::pair<ItemType, float> p : resourcePairs)
	{
		if (resources->Get(p.first) >= p.second)
			continue;
		res.push_back(p);
		endTask = false;
	}
	if (endTask)
	{
		allocator->RemoveTask(t.id);
		return;
	}

	for (std::pair<ItemType, float> r : res)
	{
		Product* p = manufacturing->GetProductTemplate(r.first);
		std::vector<std::pair<ItemType, float>> lackingResources;

		float amount = r.second;

		if (p->CanAfford(resources->Get(), lackingResources, amount))
		{
			BuildingType bType = manufacturing->GetBuildingForType(r.first);

			if (!build->HasBuilding(bType))
			{
				if (build->IsInQueue(bType))
					continue;

				Task bTask;
				bTask.type = TaskType::Build;
				bTask.buildingType = bType;
				bTask.priority = t.priority + 2;
				allocator->AddTask(bTask);

				continue;
			}

			PathNode* buildingNode = GetBuildingLocation(bType);

			if (DistanceBetween(ownerAI->GetPosition(), buildingNode->position) > ownerAI->GetRadius())
			{
				bool valid = true;
				ownerAI->GoTo(buildingNode, valid, true);
				continue;
			}
			
			p->RemoveResources(resources, amount);
			resources->Add(r.first, amount);
		}
		else
		{
			std::string s;
			Logger::Instance().Log(ownerAI->GetName() + " not enough resources to manufacture " + ToString(r.first));
			for (auto l : res)
			{
				s += std::to_string(resources->Get(l.first)) + "/" + std::to_string(l.second) + " " + ToString(l.first) + ", ";
			}
			Logger::Instance().Log(ownerAI->GetName() + " has: " + s + "\n");
			AddAcquisitionTask(lackingResources, t.priority + 1);
		}
	}
}

void AIBrain::AddAcquisitionTask(std::vector<std::pair<ItemType, float>> lackingResources, float priority)
{
	std::vector<std::pair<ItemType, float>> gatherResources;
	std::vector<std::pair<ItemType, float>> manufactureResources;

	ResourceProductionType(lackingResources, gatherResources, manufactureResources);

	if (!gatherResources.empty())
	{
		Task t;
		t.type = TaskType::Gather;
		t.resources = gatherResources;
		t.priority = priority;
		allocator->AddTask(t);
	}

	if (!manufactureResources.empty())
	{
		Task tt;
		tt.type = TaskType::Manufacture;
		tt.resources = manufactureResources;
		tt.priority = priority;
		allocator->AddTask(tt);
	}
}

void AIBrain::LogCurrentTaskList()
{
	Logger::Instance().Log("---------------------------------------------------");
	std::string s;
	for (auto l : resources->inventory)
	{
		s += std::to_string(resources->Get(l.first)) + " " + ToString(l.first) + ", ";
	}
	Logger::Instance().Log(ownerAI->GetName() + " has: " + s + "\n");

	auto r = allocator->tasks;

	std::sort(r.begin(), r.end(), [](Task a, Task b)
		{
			return a.priority > b.priority;
		});

	for (auto w : allocator->tasks)
	{
		Logger::Instance().Log(ownerAI->GetName() + " Task: " + ToString(w.type) + "(task id : " + std::to_string(w.id) + " priority = " + std::to_string(w.priority) + ")");
		std::string ss;

		if (w.type == TaskType::Gather)
		{
			for (auto l : w.resources)
			{
				ss += std::to_string(l.second) + " " + ToString(l.first) + ", ";
			}
			Logger::Instance().Log(ownerAI->GetName() + " --- requires: " + ss + "\n");
		}
		if (w.type == TaskType::Manufacture)
		{
			for (auto l : w.resources)
			{
				ss += std::to_string(l.second) + " " + ToString(l.first) + ", ";
			}
			Logger::Instance().Log(ownerAI->GetName() + " --- producing: " + ss + "\n");
		}
		if (w.type == TaskType::Build)
		{
			Logger::Instance().Log(ownerAI->GetName() + " --- building: " + ToString(w.buildingType) + "\n");
		}
	}

	Logger::Instance().Log("---------------------------------------------------\n");
}

void AIBrain::FSM(float deltaTime)
{
	frames++;
	if (frames % 300 == 0)
		LogCurrentTaskList();

	if (!ownerAI)
		return;

	if (!allocator->HasPending() && ownerAI->GetCurrentState() != GameAI::State::STATE_IDLE)
	{
		ownerAI->SetState(GameAI::State::STATE_IDLE);
		return;
	}

	Grid& grid = GameLoop::Instance().GetGrid();
	std::vector<PathNode*> nodes;

	Building* b;
	Task bTask;
	PathNode* frontier;
	std::vector<std::pair<ItemType, float>> lackingResources;
	bool foundResource;

	Task* tt = allocator->GetNext();
	if (tt == nullptr)
		return;
	Task& t = *tt;
	if (t.id != prevTaskId)
	{
		Logger::Instance().Log(ownerAI->GetName() + " assigned to " + ToString(t.type) + "(task id : " + std::to_string(t.id) + " priority = " + std::to_string(t.priority) + ")");
		if (t.type == TaskType::Gather)
		{
			std::string ss;
			for (auto l : t.resources)
			{
				ss += std::to_string(l.second) + " " + ToString(l.first) + ", ";
			}
			Logger::Instance().Log(ownerAI->GetName() + " --- requires: " + ss + "\n");
		}

	}

	switch (t.type)
	{
	case TaskType::Gather:
		GatherResources(t, deltaTime);
		break;
	case TaskType::Manufacture:
		ManufactureProducts(t, deltaTime);
		break;
	case TaskType::TrainSoldiers:
		if (t.amount <= 0)
		{
			allocator->RemoveTask(t.id);
			break;
		}

		if (build->IsInQueue(BuildingType::Barrack))
		{
			Logger::Instance().Log(ownerAI->GetName() + " waiting for barrack to train soldiers \n");
			break;
		}

		if (!build->HasBuilding(BuildingType::Barrack))
		{
			Logger::Instance().Log(ownerAI->GetName() + " no barrack to train soldiers \n");
			bTask.type = TaskType::Build;
			bTask.priority = t.priority + 1;
			bTask.buildingType = BuildingType::Barrack;
			allocator->AddTask(bTask);
			break;
		}

		if (DistanceBetween(ownerAI->GetPosition(), GetBuildingLocation(BuildingType::Barrack)->position) > ownerAI->GetRadius())
		{
			PathNode* barrackNode = GetBuildingLocation(BuildingType::Barrack);

			bool valid = true;
			ownerAI->GoTo(barrackNode, valid, true);

			// if not valid, explore
			if (!valid)
			{
				Task tt;
				tt.type = TaskType::Discover;
				tt.priority = t.priority + 1;
				tt.time = 5.0f;
				allocator->AddTask(tt);
			}
		}
		else
		{
			Soldier* s = military->GetTemplate(SoldierType::Infantry);
			int soldiersPerBatch = 20;

			if (t.amount < soldiersPerBatch)
				soldiersPerBatch = t.amount;

			if (s->CanAfford(resources->Get(), lackingResources, soldiersPerBatch))
			{
				s->RemoveResources(resources, soldiersPerBatch);
				military->TrainSoldiers(s->type, soldiersPerBatch);
				t.amount -= soldiersPerBatch;
			}
			else
			{
				Logger::Instance().Log(ownerAI->GetName() + " not enough resources to train soldier \n");

				AddAcquisitionTask(lackingResources, t.priority + 1);
			}
		}
		break;
	case TaskType::Build:

		if (build->HasBuilding(t.buildingType) || build->IsInQueue(t.buildingType))
			allocator->RemoveTask(t.id);

		b = build->GetBuildingTemplate(t.buildingType);

		if (b->CanAfford(resources->Get(), lackingResources))
		{
			if (DistanceBetween(ownerAI->GetPosition(), GetBuildingLocation(t.buildingType)->position) > ownerAI->GetRadius() + 2)
			{

				bool valid = true;
				ownerAI->GoTo(GetBuildingLocation(t.buildingType), valid, true);

				// if not valid, explore
				if (!valid)
				{
					Task tt;
					tt.type = TaskType::Discover;
					tt.priority = t.priority + 1;
					tt.time = 5.0f;
					allocator->AddTask(tt);
				}

				break;
			}

			b->RemoveResources(resources);
			build->QueueBuilding(t.buildingType, GetBuildingLocation(t.buildingType)->position);
			allocator->RemoveTask(t.id);
			break;
		}

		AddAcquisitionTask(lackingResources, t.priority + 1);

		Logger::Instance().Log(ownerAI->GetName() + " cannot afford barrack \n");


		break;
	case TaskType::Discover:

		for (std::vector<std::pair<ItemType, float>>::iterator it = t.resources.begin(); it != t.resources.end();)
		{
			if (resources->Get(it->first) >= it->second)
				it = t.resources.erase(it);
			else
				++it;
		}

		if (t.time <= 0)
		{
			Logger::Instance().Log(ownerAI->GetName() + " explored for time duration \n");
			allocator->RemoveTask(t.id);
			break;
		}
		t.time -= deltaTime;
		t.time = std::max(0.0f, t.time);

		foundResource = false;
		for (auto resource : t.resources)
		{
			for (PathNode* node : knownResources[resource.first])
			{
				if (ownerAI->CanGoTo(node))
				{
					Logger::Instance().Log(ownerAI->GetName() + " discovered resource " + ToString(resource.first) +  "\n");
					allocator->RemoveTask(t.id);
					foundResource = true;
					break;
				}
			}
		}

		if (foundResource)
			break;

		frontier = FindClosestFrontier();
		if (frontier)
		{
			bool valid = true;
			ownerAI->GoTo(frontier, valid);
			break;
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
		Logger::Instance().Log(ownerAI->GetName() + " has died.\n");
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

	if (node->clearance <= ownerAI->GetRadius())
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

	// Dijstras algorithm
	std::queue<PathNode*> q;
	std::unordered_set<PathNode*> visited;

	q.push(start);
	visited.insert(start);

	while (!q.empty())
	{
		PathNode* current = q.front();
		q.pop();

		if (IsFrontierNode(current))
		{
			if (ownerAI->CanGoTo(current))
				return current;
		}

		for (PathNode* n : current->neighbors)
		{
			int r = -1;
			int c = -1;
			grid.WorldToGrid(n->position, r, c);

			if (visited.find(n) == visited.end() && knownNodes[r][c].discovered && knownNodes[r][c].walkable)
			{
				visited.insert(n);
				q.push(n);
			}
		}
	}

	return nullptr;
}

PathNode* AIBrain::FindClosestOpenArea(Vec2 areaSize)
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

	// Dijstras algorithm
	std::queue<PathNode*> q;
	std::unordered_set<PathNode*> visited;

	q.push(start);
	visited.insert(start);

	while (!q.empty())
	{
		PathNode* current = q.front();
		q.pop();

		if (current->clearance > ownerAI->GetRadius())
		{
			int row;
			int col;
			grid.WorldToGrid(current->position, row, col);

			if (row + areaSize.x <= grid.GetRows() && col + areaSize.y <= grid.GetCols())
			{
				bool valid = true;
				for (int r = row; r < row + areaSize.x; r++)
				{
					for (int c = col; c < col + areaSize.y; c++)
					{
						if (grid.GetNodes()[r][c].type != PathNode::Type::Nothing)
							valid = false;
					}
				}

				if (valid)
					return current;
			}
		}

		for (PathNode* n : current->neighbors)
		{
			int r = -1;
			int c = -1;
			grid.WorldToGrid(n->position, r, c);

			if (visited.find(n) == visited.end() && knownNodes[r][c].walkable)
			{
				visited.insert(n);
				q.push(n);
			}
		}
	}

	return nullptr;
}

PathNode* AIBrain::GetBuildingLocation(BuildingType type)
{
	if (buildingLoc.find(type) != buildingLoc.end())
		return buildingLoc.at(type);

	Building* b = build->GetBuildingTemplate(type);
	PathNode* location = FindClosestOpenArea(b->size);
	buildingLoc[type] = location;
	return location;
}
