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
enum class TaskType { None, Explore, GatherWood, Build, Transport, MineCoal, ForgeWeapon, Smelt };

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

struct Task
{
	int id = -1;
	TaskType type = TaskType::None;
	std::vector<std::pair<ItemType, float>> resources;
	float time = 0;
	float priority = 0.0f;
	BuildingType buildingType;
	std::string meta;
	float amount = 0;
	bool completed = false;
	Building* building;
};

struct Cost
{
	float coal = 0;
	float iron = 0;
	float wood = 0;
	float iron_bar = 0;
	float sword = 0;

	bool NeedsResource(ItemType t)
	{
		if (t == ItemType::Coal && coal > 0)
		{
			coal--;
			return true;
		}
		if (t == ItemType::Iron && iron > 0)
		{
			iron--;
			return true;
		}
		if (t == ItemType::Wood && wood > 0)
		{
			wood--;
			return true;
		}
		if (t == ItemType::Iron_Bar && iron_bar > 0)
		{
			iron_bar--;
			return true;
		}
		if (t == ItemType::Sword && sword > 0)
		{
			sword--;
			return true;
		}

		return false;
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
	int AddTask(const Task& t);
	void Update(float dt);
	Task* GetNext(TaskType type);
	Task* GetNext(TaskType type, ItemType item);
	void Clear() { tasks.clear(); }

	std::map<TaskType, std::vector<Task>> tasks;
	std::vector<Task> currentTasks;
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
	Cost Get();
	void Add(ItemType r, float amount);
	bool Request(ItemType r, float amount);

	std::map<ItemType, float> inventory;
private:
	AIBrain* owner;
};

class TransportManager
{
public:
	TransportManager(AIBrain* owner);
	void Update(float dt);
	void ScheduleTransport(ItemType r, int amount, const Vec2& from, const Vec2& to);

private:
	AIBrain* owner;
	int pendingTransports = 0;
};


class Costable
{
public:
	bool CanAfford(Cost availableResources, std::vector<std::pair<ItemType, float>>& lackingResources, int amountToAfford = 1)
	{
		bool canAfford = true;
		if (availableResources.coal < cost.coal * amountToAfford)
		{
			std::pair<ItemType, float> r;
			r.first = ItemType::Coal;
			r.second = cost.coal * amountToAfford;
			lackingResources.push_back(r);
			canAfford = false;
		}
		if (availableResources.iron < cost.iron * amountToAfford)
		{
			std::pair<ItemType, float> r;
			r.first = ItemType::Iron;
			r.second = cost.iron * amountToAfford;
			lackingResources.push_back(r);
			canAfford = false;
		}
		if (availableResources.wood < cost.wood * amountToAfford)
		{
			std::pair<ItemType, float> r;
			r.first = ItemType::Wood;
			r.second = cost.wood * amountToAfford;
			lackingResources.push_back(r);
			canAfford = false;
		}
		if (availableResources.iron_bar < cost.iron_bar * amountToAfford)
		{
			std::pair<ItemType, float> r;
			r.first = ItemType::Iron_Bar;
			r.second = cost.iron_bar * amountToAfford;
			lackingResources.push_back(r);
			canAfford = false;
		}
		if (availableResources.sword < cost.sword * amountToAfford)
		{
			std::pair<ItemType, float> r;
			r.first = ItemType::Sword;
			r.second = cost.sword * amountToAfford;
			lackingResources.push_back(r);
			canAfford = false;
		}
		return canAfford;
	}

	bool RemoveResources(std::unique_ptr<ResourceManager>& resourceManager, int amount = 1)
	{
		bool affordCoal = resourceManager->Request(ItemType::Coal, cost.coal * amount);
		bool affordIron = resourceManager->Request(ItemType::Iron, cost.iron * amount);
		bool affordWood = resourceManager->Request(ItemType::Wood, cost.wood * amount);
		bool affordIron_Bar = resourceManager->Request(ItemType::Iron_Bar, cost.iron_bar * amount);
		bool affordSword = resourceManager->Request(ItemType::Sword, cost.sword * amount);

		if (affordCoal && affordIron && affordWood && affordIron_Bar && affordSword)
			return true;
		std::string s;
		if (!affordCoal)
			s += "coal, ";
		if (!affordIron)
			s += "iron, ";
		if (!affordWood)
			s += "wood, ";
		if (!affordIron_Bar)
			s += "iron_bar, ";
		if (!affordSword)
			s += "sword, ";
			
		std::cout << "Failed to remove resources: " << s << std::endl;
		return false;
	}

	float productionTime;

protected:
	Cost cost;
};

class Building : public Costable
{
public:
	Building(BuildingType build, PathNode* node) : type(build), targetNode(node)
	{
		if (build == BuildingType::Coal_Mine)
		{
			Cost c;
			c.wood = 10;
			cost = c;
			size = Vec2(2, 2);
			productionTime = 60.0f;
		}
		else if (build == BuildingType::Forge)
		{
			Cost c;
			c.iron_bar = 3;
			c.wood = 10;
			cost = c;
			size = Vec2(3, 4);
			productionTime = 180.0f;
		}
		else if (build == BuildingType::Smelter)
		{
			Cost c;
			c.wood = 10;
			cost = c;
			size = Vec2(1, 2);
			productionTime = 120.0f;
		}
		else if (build == BuildingType::Training_Camp)
		{
			Cost c;
			c.wood = 10;
			cost = c;
			size = Vec2(1, 2);
			productionTime = 120.0f;
		}
	}

	void PlaceBuilding();
	void RemoveBuilding();
	bool AddResource(ItemType resource);

	PathNode* targetNode;
	BuildingType type;
	Vec2 size;
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
		for (auto b : queue)
		{
			delete b;
		}
		for (auto b : buildingTemplates)
		{
			delete b.second;
		}
	}
	void Update(float dt);
	void WorkOnBuilding(float dt, BuildingType building);
	bool HasBuilding(const BuildingType type) const;
	Building* GetBuilding(const BuildingType type) const;
	Building* GetBuildingTemplate(const BuildingType type) const;
	bool IsInQueue(const BuildingType type) const;
	void QueueBuilding(BuildingType type, PathNode* node);

private:
	AIBrain* owner;
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
			Cost iron_bar;
			iron_bar.coal = 3;
			iron_bar.iron = 2;
			cost = iron_bar;
			productionTime = 30.0f;
		}
		else if (type == ItemType::Sword)
		{
			Cost sword;
			sword.iron_bar = 1;
			sword.coal = 2;
			cost = sword;
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
			c.sword = 1;
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

static std::string ToString(TaskType type)
{
	switch (type)
	{
	case (TaskType::None):
		return "nothing";
	case (TaskType::Explore):
		return "explore";
	case (TaskType::GatherWood):
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
