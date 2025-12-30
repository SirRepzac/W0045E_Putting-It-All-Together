#pragma once
#include "GameAI.h"
#include <memory>
#include <vector>
#include <string>

#include "AIBrainManagers.h"

class AIBrain
{
public:
	AIBrain(GameAI* owner);
	~AIBrain();

	void Think(float deltaTime);

	// Priorities
	void SetMaterialPriority(float p);
	void SetLaborPriority(float p);
	void SetConstructionPriority(float p);

	// Desires
	void AddDesire(const std::string& name, TaskType taskType, ResourceType primaryResource, int targetCount, float importance = 1.0f);

	// Expose managers for now (could switch to accessor methods)
	WorldDiscoveryManager* GetDiscovery() { return discovery.get(); }
	ResourceManager* GetResources() { return resources.get(); }
	BuildManager* GetBuild() { return build.get(); }
	MilitaryManager* GetMilitary() { return military.get(); }
	TaskAllocator* GetAllocator() { return allocator.get(); }

private:
	// internal update steps
	void UpdateValues(float deltaTime);
	void Decay(float deltaTime);
	void FSM(float deltaTime);
	void CheckDeath();

	GameAI* ownerAI = nullptr;

	float materialPriority = 1.0f;
	float laborPriority = 1.0f;
	float constructionPriority = 1.0f;

	std::vector<Desire> desires;

	std::unique_ptr<WorldDiscoveryManager> discovery;
	std::unique_ptr<ResourceManager> resources;
	std::unique_ptr<TransportManager> transport;
	std::unique_ptr<BuildManager> build;
	std::unique_ptr<ManufacturingManager> manufacturing;
	std::unique_ptr<MilitaryManager> military;
	std::unique_ptr<TaskAllocator> allocator;
};