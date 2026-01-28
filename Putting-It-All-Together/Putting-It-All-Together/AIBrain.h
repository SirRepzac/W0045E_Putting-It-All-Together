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
	float resourceAmount = 0;
	PathNode::ResourceType resource = PathNode::ResourceType::None;
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
	PathNode* approaching = nullptr;

	void Update(float dt);
	void OperateBuilding(BuildingType buildingType, ItemType toProduce, float timeToProduce, float dt);
};


class AIBrain
{
public:
	AIBrain();
	~AIBrain();

	void Think(float deltaTime);

	ResourceManager* GetResources() { return resources.get(); }
	BuildManager* GetBuild() { return build.get(); }
	PopulationManager* GetPopulation() { return population.get(); }
	TaskAllocator* GetAllocator() { return taskAllocator.get(); }
	ManufacturingManager* GetManafacturing() { return manufacturing.get(); }

	void UpdateDiscovered();

	void ExploreNode(PathNode* node, Grid& grid, double& gameTime);

	std::vector<std::vector<KnownNode>> knownNodes;
	bool IsDiscovered(int index) const;
	bool IsDiscovered(int row, int col) const;
	bool IsDiscovered(const PathNode* node);

	PathNode* FindClosestFrontier(Agent* agent);

	PathNode* homeNode;

	double lifeTime = 0;

	int discoveredAllTicks = 0;
	bool discoveredAll = false;
	std::vector<PathNode*> KnownNodesOfType(PathNode::ResourceType type);
	bool CanUseNode(const PathNode* node);
	KnownNode& NodeToKnown(const PathNode* node);
	std::map<PathNode::ResourceType, std::vector<PathNode*>> knownResources;
private:
	Agent* GetBestAgent(PopulationType type, PathNode* node);
	void UpdatePopulationTasks(float dt);
	bool TrainUnit(PopulationType type);
	void PickupNewTrained();
	void FSM(float deltaTime);
	void UpdateSystemTasks(float dt);
	void BuildBuilding(BuildingType b, PathNode* node = nullptr);
	void Gather(ItemType resource, int amount, float priority);
	void CheckDeath();

	std::map<PopulationType, std::vector<Agent*>> populationMap;
	std::vector<Agent*> agents;

	std::unique_ptr<ResourceManager> resources;
	std::unique_ptr<BuildManager> build;
	std::unique_ptr<ManufacturingManager> manufacturing;
	std::unique_ptr<PopulationManager> population;
	std::unique_ptr<TaskAllocator> taskAllocator;

	std::map<PopulationType, float> tryTraining;

	PathNode* GetBuildingLocation(BuildingType type);

	int frames = 0;

	Vec2 startPos = { 965, 491 };
};