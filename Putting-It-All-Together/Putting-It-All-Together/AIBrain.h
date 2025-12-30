#pragma once
#include "GameAI.h"
#include <memory>
#include <vector>
#include <map>
#include <string>

class AIBrain
{
public:
	// Resource types used by the strategic AI
	enum class ResourceType { Wood, Coal, Iron, None };

	// High-level task types
	enum class TaskType { None, Discover, FellTrees, Transport, Build, Manufacture, TrainSoldiers };

	// Task and desire definitions
	struct Task
	{
		int id = -1;
		TaskType type = TaskType::None;
		ResourceType resource = ResourceType::None;
		float priority =0.0f;
		bool assigned = false;
		Vec2 targetPos = Vec2(0,0);
		std::string meta;
	};

	struct Desire
	{
		std::string name;
		TaskType fulfillTaskType = TaskType::None;
		ResourceType primaryResource = ResourceType::None;
		int targetCount =0;
		int currentCount =0;
		float importance =1.0f;
	};

	// Lightweight manager structs (defined inline)
	struct WorldDiscoveryManager
	{
		WorldDiscoveryManager(AIBrain* owner) : owner(owner) {}
		void Update(float dt);
		void DiscoverAround(const Vec2& pos, float range);
		bool HasUnexplored() const { return !unexplored.empty(); }

		AIBrain* owner;
		std::vector<Vec2> unexplored;
	};

	struct ResourceManager
	{
		ResourceManager(AIBrain* owner) : owner(owner) {}
		void Update(float dt);
		int Get(ResourceType r) const;
		void Add(ResourceType r, int amount);
		bool Request(ResourceType r, int amount);

		AIBrain* owner;
		std::map<ResourceType, int> inventory;
	};

	struct TransportManager
	{
		TransportManager(AIBrain* owner) : owner(owner) {}
		void Update(float dt);
		void ScheduleTransport(ResourceType r, int amount, const Vec2& from, const Vec2& to);

		AIBrain* owner;
		int pendingTransports =0;
	};

	struct BuildManager
	{
		BuildManager(AIBrain* owner) : owner(owner) {}
		void Update(float dt);
		void QueueBuilding(const std::string& name, const Vec2& pos);
		bool HasBuilding(const std::string& name) const;

		AIBrain* owner;
		std::vector<std::string> queue;
		std::vector<std::string> builtBuildings;
	};

	struct ManufacturingManager
	{
		ManufacturingManager(AIBrain* owner) : owner(owner) {}
		void Update(float dt);
		void QueueManufacture(const std::string& item, int amount);

		AIBrain* owner;
		std::map<std::string, int> orders;
	};

	struct MilitaryManager
	{
		MilitaryManager(AIBrain* owner) : owner(owner) {}
		void Update(float dt);
		void TrainSoldiers(int count);

		AIBrain* owner;
		int soldiers =0;
		int trainingQueue =0;
	};

	struct TaskAllocator
	{
		TaskAllocator(AIBrain* owner) : owner(owner) {}
		int AddTask(const Task& t);
		void Update(float dt);
		bool HasPending() const { return !tasks.empty(); }
		Task GetNext();
		void Reprioritize();

		AIBrain* owner;
		std::vector<Task> tasks;
		int nextId =1;
	};

public:
	AIBrain(GameAI* owner);
	~AIBrain();

	void Think(float deltaTime);
	void UpdateValues(float deltaTime);
	void Decay(float deltaTime);
	void CheckDeath();
	void FSM(float deltaTime);

	// Priority setters
	void SetMaterialPriority(float p) { materialPriority = p; }
	void SetLaborPriority(float p) { laborPriority = p; }
	void SetConstructionPriority(float p) { constructionPriority = p; }

	// Desire API
	void AddDesire(const std::string& name, TaskType taskType, ResourceType primaryResource, int targetCount, float importance =1.0f);

private:
	GameAI* ownerAI = nullptr;

	// Priorities
	float materialPriority =1.0f;
	float laborPriority =1.0f;
	float constructionPriority =1.0f;

	// Managers
	std::unique_ptr<WorldDiscoveryManager> discovery;
	std::unique_ptr<ResourceManager> resources;
	std::unique_ptr<TransportManager> transport;
	std::unique_ptr<BuildManager> build;
	std::unique_ptr<ManufacturingManager> manufacturing;
	std::unique_ptr<MilitaryManager> military;
	std::unique_ptr<TaskAllocator> allocator;

	std::vector<Desire> desires;
};