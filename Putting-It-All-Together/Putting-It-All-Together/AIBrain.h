#pragma once
#include "GameAI.h"
#include <memory>
#include <vector>
#include <string>
#include <queue>
#include <unordered_set>

#include "AIBrainManagers.h"

struct KnownNode
{
	bool discovered = false;
	bool walkable = false;   // belief
	float lastSeenTime = 0;
};

class AIBrain;

class Agent
{
public:
	Agent(GameAI* ai) : ai(ai), busy(false), type(PopulationType::Worker) {}
	GameAI* ai;
	PopulationType type;
	bool busy;
	Task* currentTask = nullptr;
	float workTimer = 0.0f;
	ItemType holding = ItemType::None;
	AIBrain* brain = nullptr;

	void Update(float dt);
};


class AIBrain
{
public:
	AIBrain();
	~AIBrain();

	void Think(float deltaTime);

	// Desires
	void AddDesire(const std::string& name, TaskType taskType, ItemType primaryResource, int targetCount, float importance = 1.0f);

	ResourceManager* GetResources() { return resources.get(); }
	BuildManager* GetBuild() { return build.get(); }
	PopulationManager* GetMilitary() { return population.get(); }
	TaskAllocator* GetAllocator() { return taskAllocator.get(); }

	void UpdateDiscovered();

	void ExploreNode(PathNode* node, Grid& grid, double& gameTime);

	std::vector<std::vector<KnownNode>> knownNodes;
	bool IsDiscovered(int index) const;
	bool IsDiscovered(int row, int col) const;
	bool IsDiscovered(PathNode* node) const;

	PathNode* FindClosestFrontier(Agent* agent);

	bool discoveredAll = false;
private:
	Agent* GetBestAgent(PopulationType type, PathNode* node);
	void UpdatePopulationTasks(float dt);
	void TrainUnit(PopulationType type);
	void PickupNewTrained();
	void FSM(float deltaTime);
	void BuildBuilding(BuildingType b, PathNode* node);
	void GatherWood(int amount, float priority, Building* building);
	void CheckDeath();

	std::map<PopulationType, std::vector<Agent*>> populationMap;
	std::vector<Agent*> agents;

	float materialPriority = 1.0f;
	float laborPriority = 1.0f;
	float constructionPriority = 1.0f;

	std::vector<Desire> desires;

	std::unique_ptr<ResourceManager> resources;
	std::unique_ptr<BuildManager> build;
	std::unique_ptr<ManufacturingManager> manufacturing;
	std::unique_ptr<PopulationManager> population;
	std::unique_ptr<TaskAllocator> taskAllocator;

	std::map<ItemType, std::vector<PathNode*>> knownResources;

	int prevTaskId = -1;

	PathNode* GetBuildingLocation(BuildingType type);

	std::map<BuildingType, PathNode*> buildingLoc;

	int frames = 0;
};