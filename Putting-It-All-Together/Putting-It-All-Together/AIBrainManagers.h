#pragma once
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include "Vec2.h"
#include "PathNode.h"
#include <memory>
#include "Renderer.h"

class GameAI;
class AIBrain; // forward
struct Agent;

// High-level enums and data structures used by the managers and AIBrain
enum class ItemType { Wood, Coal, Iron, Iron_Bar, Sword, None };
enum class TaskType { None, Explore, Gather, Build, Transport, MineCoal, ForgeWeapon, Smelt };

enum class PopulationType
{
	Worker,
	Scout,
	Soldier,
	Coal_Miner,
	ArmSmith,
	Smelter,
	Builder,
	End
};

enum class BuildingType
{
	None,
	Start,
	Coal_Mine,
	Forge,
	Smelter,
	Training_Camp,
	End
};

class Building;

struct Task
{
	int id = -1;
	TaskType type = TaskType::None;
	PathNode::ResourceType resource;
	float time = 0;
	float priority = 0.0f;
	BuildingType buildingType;
	std::string meta;
	float amount = 1;
	bool completed = false;
	Building* building;
};

struct Cost
{
	std::map<ItemType, float> resources;

	bool NeedsResource(ItemType t)
	{
		for (auto res : resources)
		{
			if (res.first == t && res.second > 0)
				return true;
		}

		return false;
	}

	Cost& operator-=(const Cost& other)
	{
		for (auto res : other.resources)
		{
			resources[res.first] -= res.second;
		}
		return *this;
	}
};

struct Desire
{
	bool added = false;
	std::string name;
	TaskType fulfillTaskType = TaskType::None;
	int targetCount = 0;
	float importance = 1.0f;
};

class TaskAllocator
{
public:
	TaskAllocator(AIBrain* owner);
	~TaskAllocator() { for (Task* t : currentTasks) delete t; for (auto& pt : tasks) for (Task* ptt : pt.second) delete ptt; }
	int AddTask(const Task& t);
	void Update(float dt);
	Task* GetNext(TaskType type);
	void Clear() { tasks.clear(); }

	std::map<TaskType, std::vector<Task*>> tasks;
	std::vector<Task*> currentTasks;
private:
	AIBrain* owner;
	int nextId = 1;
};

class ResourceManager
{
public:
	ResourceManager(AIBrain* owner);
	void Update(float dt);
	float Get(ItemType r) const;
	void Add(ItemType r, float amount);
	bool Request(ItemType r, float amount);

	Cost inventory;
private:
	AIBrain* owner;
};

class Costable
{
public:
	bool CanAfford(Cost availableResources, std::vector<std::pair<ItemType, float>>& lackingResources, int amountToAfford = 1);

	bool RemoveResources(Cost availableResources, int amount = 1);

	float productionTime;

	bool HasCost();

	Cost cost;
protected:
};

class Building : public Costable
{
public:
	Building(BuildingType build, PathNode* node) : type(build), targetNode(node)
	{
		if (build == BuildingType::Coal_Mine)
		{
			Cost c;
			c.resources[ItemType::Wood] = 10;
			cost = c;
			productionTime = 60.0f;
		}
		else if (build == BuildingType::Forge)
		{
			Cost c;
			c.resources[ItemType::Iron_Bar] = 3;
			c.resources[ItemType::Wood] = 10;
			cost = c;
			productionTime = 180.0f;
		}
		else if (build == BuildingType::Smelter)
		{
			Cost c;
			c.resources[ItemType::Wood] = 10;
			cost = c;
			productionTime = 120.0f;
		}
		else if (build == BuildingType::Training_Camp)
		{
			Cost c;
			c.resources[ItemType::Wood] = 10;
			cost = c;
			productionTime = 120.0f;
		}
	}

	void WorkOnBuilding(float dt);

	void PlaceBuilding();
	void RemoveBuilding();
	bool AddResource(ItemType resource);

	PathNode* targetNode;
	BuildingType type;
	bool built = false;
	Cost inventory;
private:
};

class BuildManager
{
public:
	BuildManager(AIBrain* owner);
	~BuildManager()
	{
		for (auto b : builtBuildings)
		{
			delete b.second;
		}
		for (auto b : underConstruction)
		{
			delete b;
		}
		for (auto b : buildingTemplates)
		{
			delete b.second;
		}
		for (auto b : queue)
		{
			delete b;
		}
	}
	void Update(float dt);
	bool HasBuilding(const BuildingType type) const;
	Building* GetBuilding(const BuildingType type) const;
	Building* GetBuildingTemplate(const BuildingType type) const;
	bool IsInQueue(const BuildingType type) const;
	Building* QueueBuilding(BuildingType type, PathNode* node);

private:
	AIBrain* owner;
	std::vector<Building*> underConstruction;
	std::vector<Building*> queue;
	std::map<BuildingType, Building*> builtBuildings;
	std::map<BuildingType, Building*> buildingTemplates;

};



class Product : public Costable
{
public:

	Product(ItemType type) : type(type)
	{
		if (type == ItemType::Iron_Bar)
		{
			Cost c;
			c.resources[ItemType::Coal] = 3;
			c.resources[ItemType::Iron] = 2;
			cost = c;
			productionTime = 30.0f;
		}
		else if (type == ItemType::Sword)
		{
			Cost c;
			c.resources[ItemType::Iron_Bar] = 1;
			c.resources[ItemType::Coal] = 2;
			cost = c;
			productionTime = 60.0f;
		}
	}

private:
	ItemType type;
};

class ManufacturingManager
{
public:
	ManufacturingManager(AIBrain* owner);
	~ManufacturingManager()
	{
		for (auto p : productTemplate)
			delete p.second;
	}
	void Update(float dt);
	void QueueManufacture(const ItemType item, int amount);
	BuildingType GetBuildingForType(ItemType type);
	Product* GetProductTemplate(ItemType type);

private:
	AIBrain* owner;
	std::map<ItemType, int> orders;
	std::map<ItemType, float> orderTime;
	std::map<ItemType, Product*> productTemplate;
};

class PopulationUpgrade : public Costable
{
public:
	PopulationUpgrade(PopulationType type) : type(type)
	{
		if (type == PopulationType::Soldier)
		{
			Cost c;
			c.resources[ItemType::Sword] = 1;
			cost = c;
			productionTime = 60.0f;
			requiredBuilding = BuildingType::Training_Camp;
		}
		else if (type == PopulationType::Scout)
		{
			productionTime = 60.0f;
		}
		// all different craftsmen
		else if (type == PopulationType::Coal_Miner || type == PopulationType::ArmSmith || type == PopulationType::Smelter || type == PopulationType::Builder)
		{
			productionTime = 120.0f;
		}
	}

	PopulationType type = PopulationType::Worker;
	BuildingType requiredBuilding = BuildingType::None;
};

class PopulationManager
{
public:
	PopulationManager(AIBrain* owner);
	~PopulationManager()
	{
		for (auto s : unitTemplates)
		{
			delete s.second;
		}
	}
	void Update(float dt);
	void TrainUnit(PopulationType type, Agent* unit);

	PopulationUpgrade* GetTemplate(PopulationType type);

	std::vector<Agent*> finishedUnits;
private:
	AIBrain* owner;
	std::map<PopulationType, PopulationUpgrade*> unitTemplates;
	std::vector<std::pair<Agent*, float>> trainingQueue;
};


static ItemType ResourceToItem(PathNode::ResourceType resourceType)
{
	switch (resourceType)
	{
	case (PathNode::ResourceType::Wood):
		return ItemType::Wood;
	case (PathNode::ResourceType::Iron):
		return ItemType::Iron;
	default:
		return ItemType::None;
	}
}

static PathNode::ResourceType ItemToResource(ItemType itemType)
{
	switch (itemType)
	{
	case (ItemType::Wood):
		return PathNode::ResourceType::Wood;
	case (ItemType::Iron):
		return PathNode::ResourceType::Iron;
	default:
		return PathNode::ResourceType::None;
	}
}

static TaskType BuildingToType(BuildingType type)
{
	switch (type)
	{
	case (BuildingType::Forge):
		return TaskType::ForgeWeapon;
	case (BuildingType::Coal_Mine):
		return TaskType::MineCoal;
	case (BuildingType::Smelter):
		return TaskType::Smelt;
	default:
		return TaskType::None;
	}
}

static std::string ToString(TaskType type)
{
	switch (type)
	{
	case (TaskType::None):
		return "nothing";
	case (TaskType::Explore):
		return "explore";
	case (TaskType::Gather):
		return "gather resources";
	case (TaskType::Build):
		return "build";
	default:
		return "other";
	}
}

static std::string ToString(ItemType type)
{
	switch (type)
	{
	case (ItemType::None):
		return "nothing";
	case (ItemType::Iron):
		return "iron";
	case (ItemType::Wood):
		return "wood";
	case (ItemType::Coal):
		return "coal";
	case (ItemType::Iron_Bar):
		return "iron_bar";
	case (ItemType::Sword):
		return "sword";
	default:
		return "nothing";
	}
}

static std::string ToString(BuildingType type)
{
	switch (type)
	{
	case (BuildingType::Forge):
		return "armsmith";
	case (BuildingType::Coal_Mine):
		return "coal mine";
	case (BuildingType::Smelter):
		return "smelter";
	case (BuildingType::Training_Camp):
		return "training camp";
	default:
		return "nothing";
	}
}

static std::string ToString(PopulationType type)
{
	switch (type)
	{
	case (PopulationType::Worker):
		return "worker";
	case (PopulationType::Scout):
		return "scout";
	case (PopulationType::Soldier):
		return "soldier";
	case (PopulationType::Coal_Miner):
		return "coal miner";
	case (PopulationType::ArmSmith):
		return "armsmith";
	case (PopulationType::Smelter):
		return "smelter";
	case (PopulationType::Builder):
		return "builder";
	default:
		return "nothing";
	}
}

static void ResourceProductionType(const std::vector<std::pair<ItemType, float>>& lackingResources,
	std::vector<std::pair<ItemType, float>>& gatherResources,
	std::vector<std::pair<ItemType, float>>& manufactureItems)
{
	for (std::pair<ItemType, float> resource : lackingResources)
	{
		if (resource.first == ItemType::Wood)
			gatherResources.push_back(resource);
		else if (resource.first == ItemType::Iron)
			gatherResources.push_back(resource);
		else if (resource.first == ItemType::Coal)
			gatherResources.push_back(resource);
		else if (resource.first == ItemType::Iron_Bar)
			manufactureItems.push_back(resource);
		else if (resource.first == ItemType::Sword)
			manufactureItems.push_back(resource);
	}
}
