#pragma once
#include "GameAI.h"
#include <memory>
#include <vector>
#include <queue>
#include <map>
#include <string>
#include <functional>

class AIBrain
{
public:

	AIBrain(GameAI* owner);

	~AIBrain();

	void Think(float deltaTime);

	void UpdateValues(float deltaTime);

	void Decay(float deltaTime);

	void CheckDeath();

	void FSM(float deltaTime);

	// Basic priority setters (very simple exposed interface)
	void SetMaterialPriority(float p) { materialPriority = p; }
	void SetLaborPriority(float p) { laborPriority = p; }
	void SetConstructionPriority(float p) { constructionPriority = p; }

private:

	GameAI* ownerAI;

	// Simple needs / priorities
	float materialPriority = 1.0f; // importance of gathering materials
	float laborPriority = 1.0f; // importance of training/assigning staff
	float constructionPriority = 1.0f; // importance of building

	// Resource types used by the strategic AI
	enum class ResourceType { Wood, Coal, Iron, None };

	// High-level task types
	enum class TaskType { None, Discover, FellTrees, Transport, Build, Manufacture, TrainSoldiers };

	struct Task
	{
		int id = -1;
		TaskType type = TaskType::None;
		ResourceType resource = ResourceType::None;
		float priority = 0.0f; // higher = more important
		bool assigned = false;
		Vec2 targetPos = Vec2(0, 0);
		std::string meta;
	};

	// Simple manager interfaces. These are minimal and intentionally light-weight
	struct WorldDiscoveryManager
	{
		WorldDiscoveryManager(AIBrain* b) : brain(b) {}
		void Update(float dt);
		void DiscoverAround(const Vec2& pos, float range);
		bool HasUnexplored() const { return !unexplored.empty(); }

		AIBrain* brain;
		std::vector<Vec2> unexplored;
	};

	struct ResourceManager
	{
		ResourceManager(AIBrain* b) : brain(b) {}
		void Update(float dt);
		int Get(ResourceType r) const;
		void Add(ResourceType r, int amount);
		bool Request(ResourceType r, int amount); // reserve (returns true if available)

		AIBrain* brain;
		std::map<ResourceType, int> inventory;
	};

	struct TransportManager
	{
		TransportManager(AIBrain* b) : brain(b) {}
		void Update(float dt);
		void ScheduleTransport(ResourceType r, int amount, const Vec2& from, const Vec2& to);

		AIBrain* brain;
		int pendingTransports = 0;
	};

	struct BuildManager
	{
		BuildManager(AIBrain* b) : brain(b) {}
		void Update(float dt);
		void QueueBuilding(const std::string& name, const Vec2& pos);

		AIBrain* brain;
		std::vector<std::string> queue;
	};

	struct ManufacturingManager
	{
		ManufacturingManager(AIBrain* b) : brain(b) {}
		void Update(float dt);
		void QueueManufacture(const std::string& item, int amount);

		AIBrain* brain;
		std::map<std::string, int> orders;
	};

	struct MilitaryManager
	{
		MilitaryManager(AIBrain* b) : brain(b) {}
		void Update(float dt);
		void TrainSoldiers(int count);

		AIBrain* brain;
		int soldiers = 0;
	};

	// Task allocator & prioritizer
	struct TaskAllocator
	{
		TaskAllocator(AIBrain* b) : brain(b) {}
		int AddTask(const Task& t);
		void Update(float dt);
		bool HasPending() const;
		Task GetNext();
		void Reprioritize();

		AIBrain* brain;
		std::vector<Task> tasks;
		int nextId = 1;
	};

	// Managers
	std::unique_ptr<WorldDiscoveryManager> discovery;
	std::unique_ptr<ResourceManager> resources;
	std::unique_ptr<TransportManager> transport;
	std::unique_ptr<BuildManager> build;
	std::unique_ptr<ManufacturingManager> manufacturing;
	std::unique_ptr<MilitaryManager> military;
	std::unique_ptr<TaskAllocator> allocator;

	// Internal simple needs
	float woodNeed = 0.0f;
	float coalNeed = 0.0f;
	float ironNeed = 0.0f;

};