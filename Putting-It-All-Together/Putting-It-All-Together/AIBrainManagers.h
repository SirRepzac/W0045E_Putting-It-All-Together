#pragma once
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include "Vec2.h"
#include "PathNode.h"
#include <memory>
#include "Renderer.h"

class AIBrain; // forward

// High-level enums and data structures used by the managers and AIBrain
enum class ItemType { Wood, Coal, Iron, Steel, Sword, None };
enum class TaskType { None, Discover, Gather, Build, TrainSoldiers, Manufacture };

static ItemType ResourceToItem(PathNode::ResourceType resourceType)
{
	switch (resourceType)
	{
	case (PathNode::ResourceType::Wood):
		return ItemType::Wood;
	case (PathNode::ResourceType::Coal):
		return ItemType::Coal;
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
	case (ItemType::Coal):
		return PathNode::ResourceType::Coal;
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
	case (TaskType::Discover):
		return "explore";
	case (TaskType::Gather):
		return "gather resources";
	case (TaskType::Build):
		return "build";
	case (TaskType::TrainSoldiers):
		return "train soldiers";
	case (TaskType::Manufacture):
		return "manufacture";
	default:
		return "nothing";
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
	case (ItemType::Steel):
		return "steel";
	case (ItemType::Sword):
		return "sword";
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
		else if (resource.first == ItemType::Steel)
			manufactureItems.push_back(resource);
		else if (resource.first == ItemType::Sword)
			manufactureItems.push_back(resource);
	}
}

enum class SoldierType
{
	Infantry,
	End
};

enum class BuildingType
{
	None,
	Start,
	Barrack,
	Forge,
	Anvil,
	End
};

struct Task
{
	int id = -1;
	TaskType type = TaskType::None;
	std::vector<std::pair<ItemType, float>> resources;
	float time = 0;
	float priority = 0.0f;
	bool assigned = false;
	BuildingType buildingType;
	std::string meta;
	float amount = 0;
};

struct Cost
{
	float coal = 0;
	float iron = 0;
	float wood = 0;
	float steel = 0;
	float sword = 0;
};

struct Desire
{
	bool added = false;
	std::string name;
	TaskType fulfillTaskType = TaskType::None;
	int targetCount = 0;
	float importance = 1.0f;
};

// Managers - small public interfaces
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

static std::string ToString(BuildingType type)
{
	switch (type)
	{
	case (BuildingType::Barrack):
		return "barrack";
	case (BuildingType::Forge):
		return "forge";
	case (BuildingType::Anvil):
		return "anvil";
	default:
		return "nothing";
	}
}

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
		if (availableResources.steel < cost.steel * amountToAfford)
		{
			std::pair<ItemType, float> r;
			r.first = ItemType::Steel;
			r.second = cost.steel * amountToAfford;
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
		bool affordSteel = resourceManager->Request(ItemType::Steel, cost.steel * amount);
		bool affordSword = resourceManager->Request(ItemType::Sword, cost.sword * amount);

		if (affordCoal && affordIron && affordWood && affordSteel && affordSword)
			return true;
		std::string s;
		if (!affordCoal)
			s += "coal, ";
		if (!affordIron)
			s += "iron, ";
		if (!affordWood)
			s += "wood, ";
		if (!affordSteel)
			s += "steel, ";
		if (!affordSword)
			s += "sword, ";
			
		std::cout << "Failed to remove resources: " << s << std::endl;
		return false;
	}

protected:
	Cost cost;
};



class Building : public Costable
{
public:
	Building(BuildingType build, Vec2 pos) : type(build), position(pos), color(Renderer::White)
	{
		if (build == BuildingType::Barrack)
		{
			Cost c;
			c.iron = 20;
			c.wood = 50;
			cost = c;
			size = Vec2(2, 2);
			color = Renderer::Purple;
		}
		else if (build == BuildingType::Forge)
		{
			Cost c;
			c.iron = 20;
			c.wood = 10;
			cost = c;
			size = Vec2(3, 4);
			color = Renderer::Olive;
		}
		else if (build == BuildingType::Anvil)
		{
			Cost c;
			c.iron = 10;
			c.wood = 20;
			cost = c;
			size = Vec2(1, 2);
			color = Renderer::Maroon;
		}
	}

	void PlaceBuilding();


	void RemoveBuilding();


	Vec2 position = Vec2();
	BuildingType type;
	Vec2 size;
	std::vector<PathNode*> targetNodes;
	uint32_t color;
private:
	void GetTargetNodes();

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
	bool HasBuilding(const BuildingType type) const;
	Building* GetBuilding(const BuildingType type) const;
	Building* GetBuildingTemplate(const BuildingType type) const;
	bool IsInQueue(const BuildingType type) const;
	void QueueBuilding(BuildingType type, const Vec2& pos);

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
		if (type == ItemType::Steel)
		{
			Cost steel;
			steel.coal = 1;
			steel.iron = 2;
			cost = steel;
		}
		else if (type == ItemType::Sword)
		{
			Cost sword;
			sword.steel = 2;
			sword.wood = 1;
			cost = sword;
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
	std::map<ItemType, Product*> productTemplate;
};



class Soldier : public Costable
{
public:
	Soldier(SoldierType type) : type(type)
	{
		if (type == SoldierType::Infantry)
		{
			Cost c;
			c.sword = 1;
			c.wood = 2;
			cost = c;

		}
	}
	SoldierType type;
};

class MilitaryManager
{
public:
	MilitaryManager(AIBrain* owner);
	~MilitaryManager()
	{
		for (auto s : soldierTemplates)
		{
			delete s.second;
		}
	}
	void Update(float dt);
	void TrainSoldiers(SoldierType type, int count);

	Soldier* GetTemplate(SoldierType type);

	int GetSoldierCount();
	int GetTrainingQueue();

private:
	AIBrain* owner;
	std::map<SoldierType, Soldier*> soldierTemplates;
	std::map<SoldierType, int> trainingQueue;
	std::map<SoldierType, int> soldierCounts;
};

class TaskAllocator
{
public:
	TaskAllocator(AIBrain* owner);
	int AddTask(const Task& t);
	void Update(float dt);
	bool HasPending() const;
	Task* GetNext();
	void RemoveTask(int id);
	void Clear() { tasks.clear(); }

	std::vector<Task> tasks; 
private:
	AIBrain* owner;
	int nextId = 1;
};
