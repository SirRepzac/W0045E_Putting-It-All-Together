#include "AIBrain.h"
#include "GameLoop.h"
#include "Logger.h"
#include <algorithm>

AIBrain::AIBrain(GameAI* owner) : ownerAI(owner)
{
	discovery = std::make_unique<WorldDiscoveryManager>(this);
	resources = std::make_unique<ResourceManager>(this);
	transport = std::make_unique<TransportManager>(this);
	build = std::make_unique<BuildManager>(this);
	manufacturing = std::make_unique<ManufacturingManager>(this);
	military = std::make_unique<MilitaryManager>(this);
	allocator = std::make_unique<TaskAllocator>(this);

	// initialize some inventory
	resources->inventory[ResourceType::Wood] = 0;
	resources->inventory[ResourceType::Coal] = 0;
	resources->inventory[ResourceType::Iron] = 0;

	woodNeed = 0.0f;
	coalNeed = 0.0f;
	ironNeed = 0.0f;
}

AIBrain::~AIBrain()
{
	// unique_ptr will clean up
	ownerAI = nullptr;
}

void AIBrain::Think(float deltaTime)
{
	if (!ownerAI)
		return;
	// Simple need decay over time
	Decay(deltaTime);

	UpdateValues(deltaTime);

	// Update managers
	discovery->Update(deltaTime);
	resources->Update(deltaTime);
	transport->Update(deltaTime);
	build->Update(deltaTime);
	manufacturing->Update(deltaTime);
	military->Update(deltaTime);
	allocator->Update(deltaTime);

	FSM(deltaTime);

	CheckDeath();
}

void AIBrain::UpdateValues(float deltaTime)
{
	// Very simple need model: needs slowly increase over time and are reduced when resources available
	woodNeed += deltaTime * 0.2f;
	coalNeed += deltaTime * 0.15f;
	ironNeed += deltaTime * 0.1f;

	// If we have inventory, reduce needs
	int haveWood = resources->Get(ResourceType::Wood);
	int haveCoal = resources->Get(ResourceType::Coal);
	int haveIron = resources->Get(ResourceType::Iron);

	if (haveWood > 0 && woodNeed > 0.0f)
	{
		float used = std::min(woodNeed, static_cast<float>(haveWood));
		woodNeed -= used;
		// consume from inventory
		resources->Request(ResourceType::Wood, static_cast<int>(used));
	}
	if (haveCoal > 0 && coalNeed > 0.0f)
	{
		float used = std::min(coalNeed, static_cast<float>(haveCoal));
		coalNeed -= used;
		resources->Request(ResourceType::Coal, static_cast<int>(used));
	}
	if (haveIron > 0 && ironNeed > 0.0f)
	{
		float used = std::min(ironNeed, static_cast<float>(haveIron));
		ironNeed -= used;
		resources->Request(ResourceType::Iron, static_cast<int>(used));
	}

	// If needs exceed thresholds, create tasks
	const float needThreshold = 5.0f;
	if (woodNeed > needThreshold)
	{
		Task t;
		t.type = TaskType::FellTrees;
		t.priority = woodNeed * materialPriority;
		t.resource = ResourceType::Wood;
		allocator->AddTask(t);
		woodNeed = 0.0f; // reset a bit to avoid spamming
	}
	if (coalNeed > needThreshold)
	{
		Task t;
		t.type = TaskType::Transport;
		t.priority = coalNeed * materialPriority;
		t.resource = ResourceType::Coal;
		allocator->AddTask(t);
		coalNeed = 0.0f;
	}
	if (ironNeed > needThreshold)
	{
		Task t;
		t.type = TaskType::Transport;
		t.priority = ironNeed * materialPriority;
		t.resource = ResourceType::Iron;
		allocator->AddTask(t);
		ironNeed = 0.0f;
	}
}

// Decay of the AI's needs over time
void AIBrain::Decay(float deltaTime)
{
	// Gradually lower priorities to simulate attention shifting
	materialPriority = std::max(0.1f, materialPriority - deltaTime * 0.01f);
	laborPriority = std::max(0.1f, laborPriority - deltaTime * 0.005f);
	constructionPriority = std::max(0.1f, constructionPriority - deltaTime * 0.007f);
}

void AIBrain::CheckDeath()
{
	bool someCondition = false;
	// placeholder: no death logic yet
	if (someCondition)
	{
		Logger::Instance().Log(ownerAI->GetName() + " has died due to unmet needs.\n");
		ownerAI->SetState(GameAI::State::STATE_IDLE);
		GameLoop::Instance().ScheduleDeath(ownerAI);
	}
}

void AIBrain::FSM(float deltaTime)
{
	if (!ownerAI)
		return;

	// Very simple finite-state handling: pull next task and log it
	if (allocator->HasPending())
	{
		Task t = allocator->GetNext();
		switch (t.type)
		{
		case TaskType::FellTrees:
			Logger::Instance().Log(ownerAI->GetName() + " assigned to fell trees (task id: " + std::to_string(t.id) + ")\n");
			// create a very small behaviour change: set AI state to SEEK to a target position if set
			ownerAI->SetState(GameAI::State::STATE_SEEK);
			break;
		case TaskType::Transport:
			Logger::Instance().Log(ownerAI->GetName() + " assigned to transport resource (task id: " + std::to_string(t.id) + ")\n");
			ownerAI->SetState(GameAI::State::STATE_FOLLOW_PATH);
			break;
		default:
			break;
		}
	}
	else
	{
		// No tasks: wander or discover
		if (discovery->HasUnexplored())
		{
			ownerAI->SetState(GameAI::State::STATE_WANDER);
		}
		else
		{
			ownerAI->SetState(GameAI::State::STATE_IDLE);
		}
	}
}

// Manager implementations

void AIBrain::WorldDiscoveryManager::Update(float dt)
{
	// placeholder: nothing heavy, could scan map and add unexplored positions
}

void AIBrain::WorldDiscoveryManager::DiscoverAround(const Vec2& pos, float range)
{
	// placeholder: add a dummy unexplored point
	unexplored.push_back(pos + Vec2(range, 0.0f));
}

void AIBrain::ResourceManager::Update(float dt)
{
	// placeholder: resources might slowly be consumed or produced
}

int AIBrain::ResourceManager::Get(ResourceType r) const
{
	auto it = inventory.find(r);
	if (it == inventory.end()) return 0;
	return it->second;
}

void AIBrain::ResourceManager::Add(ResourceType r, int amount)
{
	inventory[r] += amount;
}

bool AIBrain::ResourceManager::Request(ResourceType r, int amount)
{
	auto it = inventory.find(r);
	if (it == inventory.end()) return false;
	if (it->second < amount) return false;
	it->second -= amount;
	return true;
}

void AIBrain::TransportManager::Update(float dt)
{
	// process pending transports
	if (pendingTransports > 0)
	{
		// pretend one transport completed per update
		pendingTransports = std::max(0, pendingTransports - 1);
	}
}

void AIBrain::TransportManager::ScheduleTransport(ResourceType r, int amount, const Vec2& from, const Vec2& to)
{
	(void)r; (void)amount; (void)from; (void)to;
	pendingTransports++;
}

void AIBrain::BuildManager::Update(float dt)
{
	// pretend to progress on first queued building
	if (!queue.empty())
	{
		// no-op for now
	}
}

void AIBrain::BuildManager::QueueBuilding(const std::string& name, const Vec2& pos)
{
	(void)pos;
	queue.push_back(name);
}

void AIBrain::ManufacturingManager::Update(float dt)
{
	// process orders (very simplified)
	for (auto it = orders.begin(); it != orders.end(); )
	{
		// pretend one item produced per update
		// decrement or remove
		it->second = std::max(0, it->second - 1);
		if (it->second == 0)
			it = orders.erase(it);
		else
			++it;
	}
}

void AIBrain::ManufacturingManager::QueueManufacture(const std::string& item, int amount)
{
	orders[item] += amount;
}

void AIBrain::MilitaryManager::Update(float dt)
{
	// placeholder
}

void AIBrain::MilitaryManager::TrainSoldiers(int count)
{
	soldiers += count;
}

// TaskAllocator implementations
int AIBrain::TaskAllocator::AddTask(const Task& t)
{
	Task copy = t;
	copy.id = nextId++;
	tasks.push_back(copy);
	return copy.id;
}

void AIBrain::TaskAllocator::Update(float dt)
{
	(void)dt;
	// Reprioritize each update so tasks respond to shifting priorities
	Reprioritize();
}

bool AIBrain::TaskAllocator::HasPending() const
{
	for (const Task& t : tasks)
	{
		if (!t.assigned) return true;
	}
	return false;
}

AIBrain::Task AIBrain::TaskAllocator::GetNext()
{
	Reprioritize();
	// return highest-priority unassigned task
	auto it = std::max_element(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) { return a.priority < b.priority; });
	if (it == tasks.end()) return Task();
	Task t = *it;
	// mark assigned
	for (Task& tt : tasks)
	{
		if (tt.id == t.id) { tt.assigned = true; break; }
	}
	return t;
}

void AIBrain::TaskAllocator::Reprioritize()
{
	// Simple example: boost tasks that relate to material using brain priority weights
	for (Task& t : tasks)
	{
		float base = t.priority;
		if (t.resource != ResourceType::None)
		{
			base *= brain->materialPriority;
		}
		// construction related
		if (t.type == TaskType::Build)
			base *= brain->constructionPriority;
		// labor-related (training)
		if (t.type == TaskType::TrainSoldiers)
			base *= brain->laborPriority;
		// clamp
		t.priority = std::max(0.0f, base);
	}
}
