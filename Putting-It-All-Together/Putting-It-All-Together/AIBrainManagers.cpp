#include "AIBrainManagers.h"
#include "AIBrain.h"
#include "Logger.h"
#include <algorithm>
#include <iostream>
#include "GameLoop.h"
#include "GameAI.h"


// TaskAllocator
TaskAllocator::TaskAllocator(AIBrain* owner) : owner(owner) {}
int TaskAllocator::AddTask(const Task& t)
{
	Task copy = t; 
	copy.id = nextId++; 
	tasks[t.type].push_back(copy); 
	return copy.id;
}

void TaskAllocator::Update(float dt)
{
	for (std::vector<Task>::iterator it = currentTasks.begin(); it != currentTasks.end();)
	{
		if (it->completed)
			it = currentTasks.erase(it);
		else
			it++;
	}
}

Task* TaskAllocator::GetNext(TaskType type)
{
	// If there are no tasks, return nullptr
	if (tasks[type].empty())
		return nullptr;

	// Find iterator to the highest-priority task
	auto it = std::max_element(tasks[type].begin(), tasks[type].end(), [](const Task& a, const Task& b)
		{
			return a.priority < b.priority;
		});

	if (it == tasks[type].end())
		return nullptr;

	currentTasks.push_back(*it);
	Task* returnTask = &currentTasks.back();
	it = tasks[type].erase(it);

	return returnTask;
}

Task* TaskAllocator::GetNext(TaskType type, ItemType item)
{
	// If there are no tasks, return nullptr
	if (tasks[type].empty())
		return nullptr;

	float highestPriority = -1.0f;
	Task* highestTask = nullptr;

	std::vector<Task>::iterator it;

	for (it = tasks[type].begin(); it != tasks[type].end(); it++)
	{
		bool hasItem = false;
		for (auto a : it->resources)
		{
			if (a.first == item)
			{
				hasItem = true;
				break;
			}
		}
		if (hasItem)
			break;

		it++;
	}

	if (it == tasks[type].end())
		return nullptr;

	currentTasks.push_back(*it);
	Task* returnTask = &currentTasks.back();
	it = tasks[type].erase(it);

	return returnTask;
}

// ResourceManager
ResourceManager::ResourceManager(AIBrain* owner) : owner(owner) {}
void ResourceManager::Update(float dt)
{
	(void)dt;
}
float ResourceManager::Get(ItemType r) const
{
	auto it = inventory.find(r);
	if (it == inventory.end())
		return 0;
	return it->second;
}
Cost ResourceManager::Get()
{
	Cost c;
	c.coal = Get(ItemType::Coal);
	c.iron = Get(ItemType::Iron);
	c.wood = Get(ItemType::Wood);
	c.iron_bar = Get(ItemType::Iron_Bar);
	c.sword = Get(ItemType::Sword);
	return c;
}
void ResourceManager::Add(ItemType r, float amount)
{
	inventory[r] += amount;
}
bool ResourceManager::Request(ItemType r, float amount)
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
void TransportManager::ScheduleTransport(ItemType r, int amount, const Vec2& from, const Vec2& to)
{
	(void)r; (void)amount; (void)from; (void)to;
	pendingTransports++;
}

// BuildManager
BuildManager::BuildManager(AIBrain* owner) : owner(owner) 
{
	for (int a = (int)BuildingType::Start + 1; a < (int)BuildingType::End; a++)
	{
		buildingTemplates[BuildingType(a)] = new Building(BuildingType(a), nullptr);
	}
}
void BuildManager::Update(float dt)
{
	(void)dt;
	if (queue.empty()) return;

	for (std::vector<Building*>::iterator it = queue.begin(); it != queue.end();)
	{
		if ((*it)->productionTime <= 0)
		{
			builtBuildings[(*it)->type] = *it;
			(*it)->PlaceBuilding();
			Logger::Instance().Log(std::string("Built: ") + ToString((*it)->type) + "\n");
			queue.erase(it);
		}
		else
			++it;
	}
}

void BuildManager::WorkOnBuilding(float dt, BuildingType building)
{
	for (auto b : queue)
	{
		if (b->type == building)
		{
			b->productionTime -= dt;
			break;
		}
	}
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
void BuildManager::QueueBuilding(BuildingType type, PathNode* node)
{
	Building* building = new Building(type, node);
	queue.push_back(building);
}

void Building::PlaceBuilding()
{
	if (!targetNode)
		return;

	Grid& grid = GameLoop::Instance().GetGrid();
	grid.SetNode(targetNode, PathNode::ResourceType::Building, 1);
}

void Building::RemoveBuilding()
{
	if (!targetNode)
		return;

	Grid& grid = GameLoop::Instance().GetGrid();
	grid.SetNode(targetNode, PathNode::ResourceType::None);
}

bool Building::AddResource(ItemType resource)
{
	if (cost.NeedsResource(resource))
	{
		return true;
	}
	else return false;
}

// ManufacturingManager
ManufacturingManager::ManufacturingManager(AIBrain* owner) : owner(owner) 
{
	productTemplate[ItemType::Iron_Bar] = new Product(ItemType::Iron_Bar);
	productTemplate[ItemType::Sword] = new Product(ItemType::Sword);
}

void ManufacturingManager::Update(float dt)
{
	for (auto order : orders)
	{
		if (order.second > 0 && orderTime[order.first] > productTemplate[order.first]->productionTime)
		{
			owner->GetResources()->Add(order.first, 1);
			order.second--;
			orderTime[order.first] = 0;
		}
	}
}

void ManufacturingManager::QueueManufacture(const ItemType item, int amount)
{
	orders[item] += amount;
}

BuildingType ManufacturingManager::GetBuildingForType(ItemType type)
{
	if (type == ItemType::Iron_Bar)
		return BuildingType::Smelter;
	else if (type == ItemType::Sword)
		return BuildingType::Forge;
	else if (type == ItemType::Coal)
		return BuildingType::Coal_Mine;
	else
		return BuildingType::None;
}

Product* ManufacturingManager::GetProductTemplate(ItemType type)
{
	return productTemplate[type];
}

// MilitaryManager
PopulationManager::PopulationManager(AIBrain* owner) : owner(owner) 
{
	for (int a = 0; a < (int)PopulationType::End; a++)
	{
		unitTemplates[PopulationType(a)] = new PopulationUpgrade(PopulationType(a));
	}
}
void PopulationManager::Update(float dt)
{
		for (auto it = trainingQueue.begin(); it != trainingQueue.end(); )
		{
			if (it->second <= 0)
			{
				finishedUnits.push_back(it->first);
				trainingQueue.erase(it++);
				Logger::Instance().Log(std::string("Trained Unit") + "\n");
			}
			else
			{
				it->second -= dt;
				++it;
			}
		}
}
void PopulationManager::TrainUnit(PopulationType type, Agent* unit)
{
	trainingQueue.push_back(std::pair<Agent*, float>(unit, unitTemplates[type]->productionTime));
}


PopulationUpgrade* PopulationManager::GetTemplate(PopulationType type)
{
	if (unitTemplates.count(type) > 0)
		return unitTemplates.at(type);
	// building not found
	Logger::Instance().Log("Failed to get soldier template \n");
	std::cout << "failed to get soldier template" << std::endl;
	return nullptr;
}
