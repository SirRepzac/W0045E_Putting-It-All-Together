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
enum class ResourceType { Wood, Coal, Iron, None };
enum class TaskType { None, Discover, Gather, Transport, Build, TrainSoldiers, Manafacture };

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
		return "exploring";
	case (TaskType::Gather):
		return "gathering resources";
	case (TaskType::Transport):
		return "transport";
	case (TaskType::Build):
		return "build";
	case (TaskType::TrainSoldiers):
		return "train soldiers";
	case (TaskType::Manafacture):
		return "manafacture";
	default:
		return "nothing";
	}
}

enum class SoldierType
{
	Infantry,
	End
};

enum class BuildingType
{
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
	Vec2 destination = Vec2();
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
	int Get(ResourceType r) const;
	Cost Get();
	void Add(ResourceType r, float amount);
	bool Request(ResourceType r, float amount);

private:
	AIBrain* owner;
	std::map<ResourceType, float> inventory;
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
	bool CanAfford(Cost availableResources, std::vector<std::pair<ResourceType, float>>& lackingResources)
	{
		bool canAfford = true;
		if (availableResources.coal < cost.coal)
		{
			std::pair<ResourceType, float> r;
			r.first = ResourceType::Coal;
			r.second = cost.coal;
			lackingResources.push_back(r);
			canAfford = false;
		}
		if (availableResources.iron < cost.iron)
		{
			std::pair<ResourceType, float> r;
			r.first = ResourceType::Iron;
			r.second = cost.iron;
			lackingResources.push_back(r);
			canAfford = false;
		}
		if (availableResources.wood < cost.wood)
		{
			std::pair<ResourceType, float> r;
			r.first = ResourceType::Wood;
			r.second = cost.wood;
			lackingResources.push_back(r);
			canAfford = false;
		}
		return canAfford;
	}

	bool RemoveResources(std::unique_ptr<ResourceManager>& resourceManager)
	{
		bool affordCoal = resourceManager->Request(ResourceType::Coal, cost.coal);
		bool affordIron = resourceManager->Request(ResourceType::Iron, cost.iron);
		bool affordWood = resourceManager->Request(ResourceType::Wood, cost.wood);

		if (affordCoal && affordIron && affordWood)
			return true;
		std::cout << "Failed to remove resources" << std::endl;
		return false;
	}

protected:
	Cost cost;
};

class Building : public Costable
{
public:
	Building(BuildingType build, Vec2 pos) : type(build), position(pos)
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
	}

	void PlaceBuilding();


	void RemoveBuilding();


	Vec2 position = 0;
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

class ManufacturingManager
{
public:
	ManufacturingManager(AIBrain* owner);
	void Update(float dt);
	void QueueManufacture(const std::string& item, int amount);

private:
	AIBrain* owner;
	std::map<std::string, int> orders;
};



class Soldier : public Costable
{
public:
	Soldier(SoldierType type) : type(type)
	{
		if (type == SoldierType::Infantry)
		{
			Cost c;
			c.iron = 1;
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

private:
	AIBrain* owner;
	std::vector<Task> tasks;
	int nextId = 1;
};
