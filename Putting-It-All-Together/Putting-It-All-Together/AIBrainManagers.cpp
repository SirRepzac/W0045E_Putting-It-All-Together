#include "AIBrainManagers.h"
#include "AIBrain.h"
#include "Logger.h"
#include <algorithm>
#include <iostream>
#include "GameLoop.h"

// ResourceManager
ResourceManager::ResourceManager(AIBrain* owner) : owner(owner) {}
void ResourceManager::Update(float dt)
{
	(void)dt;
}
float ResourceManager::Get(ResourceType r) const
{
	auto it = inventory.find(r);
	if (it == inventory.end())
		return 0;
	return it->second;
}
Cost ResourceManager::Get()
{
	Cost c;
	c.coal = Get(ResourceType::Coal);
	c.iron = Get(ResourceType::Iron);
	c.wood = Get(ResourceType::Wood);
	c.steel = Get(ResourceType::Steel);
	c.sword = Get(ResourceType::Sword);
	return c;
}
void ResourceManager::Add(ResourceType r, float amount)
{
	inventory[r] += amount;
}
bool ResourceManager::Request(ResourceType r, float amount)
{
	if (amount <= 0)
		return true;
	auto it = inventory.find(r);
	if (it == inventory.end())
		return false;
	if (it->second < amount)
		return false;
	it->second -= amount;
	return true;
}

// TransportManager
TransportManager::TransportManager(AIBrain* owner) : owner(owner) {}
void TransportManager::Update(float dt)
{
	(void)dt;
	if (pendingTransports > 0)
		pendingTransports = std::max(0, pendingTransports - 1);
}
void TransportManager::ScheduleTransport(ResourceType r, int amount, const Vec2& from, const Vec2& to)
{
	(void)r; (void)amount; (void)from; (void)to;
	pendingTransports++;
}

// BuildManager
BuildManager::BuildManager(AIBrain* owner) : owner(owner) 
{
	for (int a = (int)BuildingType::Start + 1; a < (int)BuildingType::End; a++)
	{
		buildingTemplates[BuildingType(a)] = new Building(BuildingType(a), Vec2());
	}
}
void BuildManager::Update(float dt)
{
	(void)dt;
	if (queue.empty()) return;
	Building* building = queue.front();
	queue.erase(queue.begin());

	builtBuildings[building->type] = building;
	building->PlaceBuilding();

	Logger::Instance().Log(std::string("Built: ") + ToString(building->type) + "\n");
}

bool BuildManager::HasBuilding(const BuildingType type) const
{
	if (builtBuildings.count(type) > 0)
		return true;
	return false;
}

Building* BuildManager::GetBuilding(const BuildingType type) const
{
	if (builtBuildings.count(type) > 0)
		return builtBuildings.at(type);
	// building not found
	Logger::Instance().Log("Failed to get building \n");
	std::cout << "failed to get building" << std::endl;
	return nullptr;
}

Building* BuildManager::GetBuildingTemplate(const BuildingType type) const
{
	if (buildingTemplates.count(type) > 0)
		return buildingTemplates.at(type);
	// building not found
	Logger::Instance().Log("Failed to get building template \n");
	std::cout << "failed to get building template" << std::endl;
	return nullptr;
}

bool BuildManager::IsInQueue(const BuildingType type) const
{
	bool found = false;
	for (auto b : queue)
	{
		if (b->type == type)
		{
			found = true;
			break;
		}
	}
	return found;
}
void BuildManager::QueueBuilding(BuildingType type, const Vec2& pos)
{
	Building* building = new Building(type, pos);
	queue.push_back(building);
}

// ManufacturingManager
ManufacturingManager::ManufacturingManager(AIBrain* owner) : owner(owner) 
{
	productTemplate[ResourceType::Steel] = new Product(ResourceType::Steel);
	productTemplate[ResourceType::Sword] = new Product(ResourceType::Sword);

}
void ManufacturingManager::Update(float dt)
{
	(void)dt;
	for (auto it = orders.begin(); it != orders.end(); )
	{
		it->second = std::max(0, it->second - 1);
		if (it->second == 0)
			it = orders.erase(it);
		else ++it;
	}
}
void ManufacturingManager::QueueManufacture(const ResourceType item, int amount)
{
	orders[item] += amount;
}

BuildingType ManufacturingManager::GetBuildingForType(ResourceType type)
{
	if (type == ResourceType::Steel)
		return BuildingType::Forge;
	else if (type == ResourceType::Sword)
		return BuildingType::Anvil;
	else
		return BuildingType::None;
}

Product* ManufacturingManager::GetProductTemplate(ResourceType type)
{
	return productTemplate[type];
}

// MilitaryManager
MilitaryManager::MilitaryManager(AIBrain* owner) : owner(owner) 
{
	for (int a = 0; a < (int)SoldierType::End; a++)
	{
		soldierTemplates[SoldierType(a)] = new Soldier(SoldierType(a));
	}
}
void MilitaryManager::Update(float dt)
{
	(void)dt;
	if (GetTrainingQueue() > 0)
	{
		for (auto s : trainingQueue)
		{
			if (s.second > 0)
			{
				trainingQueue[s.first] -= 1;
				soldierCounts[s.first] += 1;
				break;
			}
		}

		Logger::Instance().Log(std::string("Trained soldier. Total now: ") + std::to_string(GetSoldierCount()) + "\n");
	}
}
void MilitaryManager::TrainSoldiers(SoldierType type, int count) 
{
	trainingQueue[type] += count;
}

Soldier* MilitaryManager::GetTemplate(SoldierType type)
{
	if (soldierTemplates.count(type) > 0)
		return soldierTemplates.at(type);
	// building not found
	Logger::Instance().Log("Failed to get soldier template \n");
	std::cout << "failed to get soldier template" << std::endl;
	return nullptr;
}

int MilitaryManager::GetSoldierCount()
{
	int total = 0;
	for (auto a : soldierCounts)
	{
		total += a.second;
	}
	return total;
}

int MilitaryManager::GetTrainingQueue()
{
	int total = 0;
	for (auto a : trainingQueue)
	{
		total += a.second;
	}
	return total;
}

// TaskAllocator
TaskAllocator::TaskAllocator(AIBrain* owner) : owner(owner) {}
int TaskAllocator::AddTask(const Task& t)
{
	Task copy = t; copy.id = nextId++; tasks.push_back(copy); return copy.id;

	Logger::Instance().Log("Task List: \n");
	for (Task ttt : tasks)
	{
		Logger::Instance().Log(ToString(ttt.type) + " task id : " + std::to_string(ttt.id) + " priority = " + std::to_string(ttt.priority));
	}
}

void TaskAllocator::Update(float dt)
{
	(void)dt;
}
bool TaskAllocator::HasPending() const
{
	return !tasks.empty();
}

Task* TaskAllocator::GetNext()
{
	// If there are no tasks, return nullptr
	if (tasks.empty())
		return nullptr;

	// Find iterator to the highest-priority task
	auto it = std::max_element(tasks.begin(), tasks.end(), [](const Task& a, const Task& b)
		{
			return a.priority < b.priority;
		});

	if (it == tasks.end())
		return nullptr;

	// Mark the actual task stored in the vector as assigned
	it->assigned = true;

	// Return pointer to the actual Task so caller can edit it in-place
	return &(*it);
}

void TaskAllocator::RemoveTask(int id)
{
	tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [id](const Task& t) { return t.id == id; }), tasks.end());
}

void Building::PlaceBuilding()
{
	if (targetNodes.size() == 0)
		GetTargetNodes();

	Grid& grid = GameLoop::Instance().GetGrid();

	for (PathNode* node : targetNodes)
	{
		if (!node)
			continue;

		grid.SetNode(node, PathNode::Special, color);
	}
}

void Building::RemoveBuilding()
{
	if (targetNodes.size() == 0)
		return;

	Grid& grid = GameLoop::Instance().GetGrid();

	for (PathNode* node : targetNodes)
	{
		grid.SetNode(node, PathNode::Nothing, Renderer::White);
	}
}

void Building::GetTargetNodes()
{
	if (targetNodes.size() > 0)
		return;

	Grid& grid = GameLoop::Instance().GetGrid();
	Vec2 baseBarrackLoc = position;

	for (int x = 0; x < size.y; x++)
	{
		PathNode* nodeX = grid.GetNodeAt(baseBarrackLoc + Vec2(DEFAULT_CELL_SIZE, 0) * x);
		targetNodes.push_back(nodeX);
	}

	int s = targetNodes.size();

	for (int e = 0; e < s; e++)
	{
		for (int y = 1; y < size.x; y++)
		{
			PathNode* nodeY = grid.GetNodeAt(targetNodes[e]->position + Vec2(0, DEFAULT_CELL_SIZE) * y);
			targetNodes.push_back(nodeY);
		}
	}

	position = baseBarrackLoc + Vec2((size.y - 1) * DEFAULT_CELL_SIZE, (size.x - 1) * DEFAULT_CELL_SIZE);
}
