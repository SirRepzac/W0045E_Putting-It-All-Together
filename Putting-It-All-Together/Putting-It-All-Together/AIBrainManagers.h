#pragma once
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include "Vec2.h"
#include "PathNode.h"

class AIBrain; // forward

// Costs and constants
static const int COST_WOOD_PER_SOLDIER = 2;
static const int COST_IRON_PER_SOLDIER = 1;
static const int COST_COAL_PER_SOLDIER = 0;

static const int BARRACK_WOOD_COST = 50;
static const int BARRACK_IRON_COST = 20;

// High-level enums and data structures used by the managers and AIBrain
enum class ResourceType { Wood, Coal, Iron, None };
enum class TaskType { None, Discover, FellTrees, Transport, Build, MineCoal, MineIron, TrainSoldiers, Manafacture };

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

static std::string ToString(TaskType type)
{
	switch (type)
	{
	case (TaskType::None):
		return "nothing";
	case (TaskType::Discover):
		return "exploring";
	case (TaskType::FellTrees):
		return "chop wood";
	case (TaskType::Transport):
		return "transport";
	case (TaskType::Build):
		return "build";
	case (TaskType::MineCoal):
		return "mine coal";
	case (TaskType::MineIron):
		return "mine iron";
	case (TaskType::TrainSoldiers):
		return "train soldiers";
	case (TaskType::Manafacture):
		return "manafacture";
	default:
		return "nothing";
	}
}

struct Task
{
	int id = -1;
	TaskType type = TaskType::None;
	ResourceType resource = ResourceType::None;
	float priority = 0.0f;
	bool assigned = false;
	std::string meta;
	int amount = 0;
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
class WorldDiscoveryManager
{
public:
	WorldDiscoveryManager(AIBrain* owner);
	void Update(float dt);
	void DiscoverAround(const Vec2& pos, float range);
	bool HasUnexplored() const;

private:
	AIBrain* owner;
	std::vector<Vec2> unexplored;
};

class ResourceManager
{
public:
	ResourceManager(AIBrain* owner);
	void Update(float dt);
	int Get(ResourceType r) const;
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

class BuildManager
{
public:
	BuildManager(AIBrain* owner);
	void Update(float dt);
	void QueueBuilding(const std::string& name, const Vec2& pos);
	bool HasBuilding(const std::string& name) const;
	bool IsInQueue(const std::string& name) const;
	Vec2 GetBuildingLocation(const std::string& name);

private:
	AIBrain* owner;
	std::vector<std::string> queue;
	std::vector<std::string> builtBuildings;
	std::map<std::string, Vec2> buildingLocations;
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

class MilitaryManager
{
public:
	MilitaryManager(AIBrain* owner);
	void Update(float dt);
	void TrainSoldiers(int count);

	int GetSoldierCount() const { return soldiers; }
	int GetTrainingQueue() const { return trainingQueue; }

private:
	AIBrain* owner;
	int soldiers = 0;
	int trainingQueue = 0;
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
