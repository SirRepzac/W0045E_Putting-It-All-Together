#pragma once
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include "Vec2.h"
#include "PathNode.h"
#include <memory>

class AIBrain; // forward

// High-level enums and data structures used by the managers and AIBrain
enum class ResourceType { Wood, Coal, Iron, Steel, Sword, None };
enum class TaskType { None, Discover, Gather, Build, TrainSoldiers, Manufacture };

static PathNode::Type ResourceToNode(ResourceType resourceType)
{
	switch (resourceType)
	{
	case (ResourceType::Wood):
		return PathNode::Type::Wood;
	case (ResourceType::Coal):
		return PathNode::Type::Coal;
	case (ResourceType::Iron):
		return PathNode::Type::Iron;
	default:
		return PathNode::Type::Nothing;
	}
}

static ResourceType NodeToResource(PathNode::Type nodeType)
{
	switch (nodeType)
	{
	case (PathNode::Type::Wood):
		return ResourceType::Wood;
	case (PathNode::Type::Coal):
		return ResourceType::Coal;
	case (PathNode::Type::Iron):
		return ResourceType::Iron;
	default:
		return ResourceType::None;
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

static std::string ToString(ResourceType type)
{
	switch (type)
	{
	case (ResourceType::None):
		return "nothing";
	case (ResourceType::Iron):
		return "iron";
	case (ResourceType::Wood):
		return "wood";
	case (ResourceType::Coal):
		return "coal";
	case (ResourceType::Steel):
		return "steel";
	case (ResourceType::Sword):
		return "sword";
	default:
		return "nothing";
	}
}

static void ResourceProductionType(const std::vector<std::pair<ResourceType, float>>& lackingResources, 
	std::vector<std::pair<ResourceType, float>>& gatherResources,
	std::vector<std::pair<ResourceType, float>>& manufactureResources)
{
	for (std::pair<ResourceType, float> resource : lackingResources)
	{
		if (resource.first == ResourceType::Wood)
			gatherResources.push_back(resource);
		else if (resource.first == ResourceType::Iron)
			gatherResources.push_back(resource);
		else if (resource.first == ResourceType::Coal)
			gatherResources.push_back(resource);
		else if (resource.first == ResourceType::Steel)
			manufactureResources.push_back(resource);
		else if (resource.first == ResourceType::Sword)
			manufactureResources.push_back(resource);
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
	std::vector<std::pair<ResourceType, float>> resources;
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
	float Get(ResourceType r) const;
	Cost Get();
	void Add(ResourceType r, float amount);
	bool Request(ResourceType r, float amount);

	std::map<ResourceType, float> inventory;
private:
	AIBrain* owner;
};

class TransportManager
{
public:
	TransportManager(AIBrain* owner);
	void Update(float dt);
	void ScheduleTransport(ResourceType r, int amount, const Vec2& from, const Vec2& to);

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
	bool CanAfford(Cost availableResources, std::vector<std::pair<ResourceType, float>>& lackingResources, int amountToAfford = 1)
	{
		bool canAfford = true;
		if (availableResources.coal < cost.coal * amountToAfford)
		{
			std::pair<ResourceType, float> r;
			r.first = ResourceType::Coal;
			r.second = cost.coal * amountToAfford;
			lackingResources.push_back(r);
			canAfford = false;
		}
		if (availableResources.iron < cost.iron * amountToAfford)
		{
			std::pair<ResourceType, float> r;
			r.first = ResourceType::Iron;
			r.second = cost.iron * amountToAfford;
			lackingResources.push_back(r);
			canAfford = false;
		}
		if (availableResources.wood < cost.wood * amountToAfford)
		{
			std::pair<ResourceType, float> r;
			r.first = ResourceType::Wood;
			r.second = cost.wood * amountToAfford;
			lackingResources.push_back(r);
			canAfford = false;
		}
		if (availableResources.steel < cost.steel * amountToAfford)
		{
			std::pair<ResourceType, float> r;
			r.first = ResourceType::Steel;
			r.second = cost.steel * amountToAfford;
			lackingResources.push_back(r);
			canAfford = false;
		}
		if (availableResources.sword < cost.sword * amountToAfford)
		{
			std::pair<ResourceType, float> r;
			r.first = ResourceType::Sword;
			r.second = cost.sword * amountToAfford;
			lackingResources.push_back(r);
			canAfford = false;
		}
		return canAfford;
	}

	bool RemoveResources(std::unique_ptr<ResourceManager>& resourceManager, int amount = 1)
	{
		bool affordCoal = resourceManager->Request(ResourceType::Coal, cost.coal * amount);
		bool affordIron = resourceManager->Request(ResourceType::Iron, cost.iron * amount);
		bool affordWood = resourceManager->Request(ResourceType::Wood, cost.wood * amount);
		bool affordSteel = resourceManager->Request(ResourceType::Steel, cost.steel * amount);
		bool affordSword = resourceManager->Request(ResourceType::Sword, cost.sword * amount);

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

	Product(ResourceType type) : type(type)
	{
		if (type == ResourceType::Steel)
		{
			Cost steel;
			steel.coal = 1;
			steel.iron = 2;
			cost = steel;
		}
		else if (type == ResourceType::Sword)
		{
			Cost sword;
			sword.steel = 2;
			sword.wood = 1;
			cost = sword;
		}
	}
private:
	ResourceType type;
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
	void QueueManufacture(const ResourceType item, int amount);
	BuildingType GetBuildingForType(ResourceType type);
	Product* GetProductTemplate(ResourceType type);

private:
	AIBrain* owner;
	std::map<ResourceType, int> orders;
	std::map<ResourceType, Product*> productTemplate;
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
