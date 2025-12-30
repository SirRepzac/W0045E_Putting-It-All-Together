#pragma once
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include "Vec2.h"

class AIBrain; // forward

// Costs and constants
static const int COST_WOOD_PER_SOLDIER = 2;
static const int COST_IRON_PER_SOLDIER = 1;
static const int COST_COAL_PER_SOLDIER = 0;

static const int BARRACK_WOOD_COST = 50;
static const int BARRACK_IRON_COST = 20;

// High-level enums and data structures used by the managers and AIBrain
enum class ResourceType { Wood, Coal, Iron, None };
enum class TaskType { None, Discover, FellTrees, Transport, Build, Manufacture, TrainSoldiers };

struct Task
{
	int id = -1;
	TaskType type = TaskType::None;
	ResourceType resource = ResourceType::None;
	float priority = 0.0f;
	bool assigned = false;
	Vec2 targetPos = Vec2(0, 0);
	std::string meta;
};

struct Desire
{
	std::string name;
	TaskType fulfillTaskType = TaskType::None;
	ResourceType primaryResource = ResourceType::None;
	int targetCount = 0;
	int currentCount = 0;
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
	void Add(ResourceType r, int amount);
	bool Request(ResourceType r, int amount);

private:
	AIBrain* owner;
	std::map<ResourceType, int> inventory;
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

private:
	AIBrain* owner;
	std::vector<std::string> queue;
	std::vector<std::string> builtBuildings;
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
	Task GetNext();
	void Reprioritize();

private:
	AIBrain* owner;
	std::vector<Task> tasks;
	int nextId = 1;
};
