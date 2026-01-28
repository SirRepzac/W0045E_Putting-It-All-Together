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
	for (int i = 0; i < t.amount; i++)
	{
		Task* copy = new Task(t);
		copy->id = nextId++;
		tasks[t.type].push_back(copy);
	}
	
	//std::cout << "Added task: " << ToString(t.type);
	//if (t.resource != ItemType::None)
	//	std::cout << " of type " << ToString(t.resource) << " x" << t.amount;
	//if (t.building)
	//	std::cout << " to building " << ToString(t.building->type);
	//else if (t.resourceTo != BuildingType::None)
	//	std::cout << " to building " << ToString(t.resourceTo);
	//if (t.resourceFrom != BuildingType::None)
	//	std::cout << " from " << ToString(t.resourceFrom) << "\n";
	//else
	//	std::cout << " from gathering \n";
	
	return nextId;
}

void TaskAllocator::Update(float dt)
{
	for (std::vector<Task*>::iterator it = currentTasks.begin(); it != currentTasks.end();)
	{
		if ((*it)->completed)
		{
			Task* finishedTask = *it;
			it = currentTasks.erase(it);
			delete finishedTask;
		}
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
	float highestPriority = -1.0f;
	std::vector<Task*>::iterator bestIt = tasks[type].end();
	for (std::vector<Task*>::iterator it = tasks[type].begin(); it != tasks[type].end(); it++)
	{
		if ((*it)->priority > highestPriority)
		{
			highestPriority = (*it)->priority;
			bestIt = it;
		}
	}

	if (bestIt == tasks[type].end())
		return nullptr;

	Task* bestTask = *bestIt;

	currentTasks.push_back(bestTask);
	tasks[type].erase(bestIt);

	return bestTask;
}

void TaskAllocator::Clear()
{
	// delete current tasks
	for (Task* t : currentTasks)
		delete t;
	currentTasks.clear();

	// delete queued tasks
	for (auto& kv : tasks)
	{
		for (Task* t : kv.second)
			delete t;
		kv.second.clear();
	}
	tasks.clear();
}

// ResourceManager
ResourceManager::ResourceManager(AIBrain* owner) : owner(owner) {}
void ResourceManager::Update(float dt)
{
	(void)dt;
}
float ResourceManager::Get(ItemType r) const
{
	auto it = inventory.resources.find(r);
	if (it == inventory.resources.end())
		return 0;
	return it->second;
}
void ResourceManager::Add(ItemType r, float amount)
{
	inventory.resources[r] += amount;
}
bool ResourceManager::Request(ItemType r, float amount)
{
	if (amount <= 0)
		return true;
	auto it = inventory.resources.find(r);
	if (it == inventory.resources.end())
		return false;
	if (it->second < amount)
		return false;
	it->second -= amount;
	return true;
}

// BuildManager
BuildManager::BuildManager(AIBrain* owner) : owner(owner)
{
	for (int a = (int)BuildingType::Start + 1; a < (int)BuildingType::End; a++)
	{
		buildingTemplates[BuildingType(a)] = new Building(BuildingType(a), nullptr);
	}

	builtBuildings[BuildingType::Storage] = new Building(BuildingType::Storage, owner->homeNode);
	builtBuildings[BuildingType::Storage]->built = true;
}
void BuildManager::Update(float dt)
{

	for (std::vector<Building*>::iterator it = queue.begin(); it != queue.end();)
	{
		if ((*it)->RemoveResources((*it)->inventory, 1))
		{
			underConstruction.push_back(*it);
			Logger::Instance().Log(std::string("All resources added for: ") + ToString((*it)->type) + "\n");
			(*it)->cost -= buildingTemplates[(*it)->type]->cost;

			Task t;
			t.type = TaskType::Build;
			t.resourceTo = (*it)->type;
			t.priority = 1.0f;
			owner->GetAllocator()->AddTask(t);

			it = queue.erase(it);
		}
		else
			++it;
	}

	for (std::vector<Building*>::iterator it = underConstruction.begin(); it != underConstruction.end();)
	{
		if ((*it)->productionTime <= 0)
		{
			builtBuildings[(*it)->type] = *it;
			(*it)->PlaceBuilding();
			Logger::Instance().Log(std::string("Built: ") + ToString((*it)->type) + "\n");
			(*it)->built = true;

			owner->GetAllocator()->AddTask((*it)->activationTask);

			it = underConstruction.erase(it);
		}
		else
			++it;
	}
}

void Building::WorkOnBuilding(float dt)
{
	if (HasCost())
		return;

	productionTime -= dt;
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
	for (auto b : underConstruction)
		if (b->type == type)
			return b;
	for (auto b : queue)
		if (b->type == type)
			return b;

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
	for (auto b : underConstruction)
	{
		if (b->type == type)
		{
			return true;
		}
	}
	for (auto b : queue)
	{
		if (b->type == type)
		{
			return true;
		}
	}
	return false;
}
Building* BuildManager::FromUnderConstruction(const BuildingType type)
{
	for (auto building : underConstruction)
	{
		if (building->type == type)
			return building;
	}

	return nullptr;
}
Building* BuildManager::QueueBuilding(BuildingType type, PathNode* node)
{
	Building* building = new Building(type, node);
	queue.push_back(building);
	return building;
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
	inventory.resources[resource]++;
	return true;
}

bool Building::TakeResource(ItemType resource)
{
	if (inventory.resources[resource] > 0)
	{
		inventory.resources[resource]--;
		return true;
	}
	return false;
}

// ManufacturingManager
ManufacturingManager::ManufacturingManager(AIBrain* owner) : owner(owner)
{
	productTemplate[ItemType::Iron_Bar] = new Product(ItemType::Iron_Bar);
	productTemplate[ItemType::Sword] = new Product(ItemType::Sword);
	productTemplate[ItemType::Coal] = new Product(ItemType::Coal);
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
	if (productTemplate.find(type) == productTemplate.end())
		return nullptr;
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

			Agent* a = it->first;

			if (a->type == PopulationType::Soldier)
				a->ai->SetColor(Renderer::Red);

			Logger::Instance().Log("Trained unit: " + ToString(a->type) + "\n");
			it = trainingQueue.erase(it);
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
	unit->type = type;
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

bool Costable::CanAfford(const Cost& availableResources,
	std::vector<std::pair<ItemType, float>>& lackingResources,
	int amountToAfford)
{
	bool canAfford = true;
	for (auto& res : cost.resources)
	{
		float required = res.second * amountToAfford;
		auto it = availableResources.resources.find(res.first);
		float have = (it != availableResources.resources.end()) ? it->second : 0.0f;
		if (have < required)
		{
			lackingResources.emplace_back(res.first, required - have);
			canAfford = false;
		}
	}
	return canAfford;
}

bool Costable::RemoveResources(Cost& availableResources, int amount)
{
	std::vector<std::pair<ItemType, float>> lacking;
	if (!CanAfford(availableResources, lacking, amount))
		return false;

	for (auto& res : cost.resources)
	{
		availableResources.resources[res.first] -= res.second * amount;
	}
	return true;
}

bool Costable::HasCost()
{
	for (auto& res : cost.resources)
	{
		if (res.second > 0)
			return true;
	}
	return false;
}
