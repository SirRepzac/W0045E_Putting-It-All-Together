#include "AIBrainManagers.h"
#include "AIBrain.h"
#include "Logger.h"
#include <algorithm>

// WorldDiscoveryManager
WorldDiscoveryManager::WorldDiscoveryManager(AIBrain* owner) : owner(owner) {}
void WorldDiscoveryManager::Update(float dt)
{
	(void)dt;
}
void WorldDiscoveryManager::DiscoverAround(const Vec2& pos, float range)
{
	unexplored.push_back(pos + Vec2(range, 0.0f));
}
bool WorldDiscoveryManager::HasUnexplored() const
{
	return !unexplored.empty();
}

// ResourceManager
ResourceManager::ResourceManager(AIBrain* owner) : owner(owner) {}
void ResourceManager::Update(float dt)
{
	(void)dt;
}
int ResourceManager::Get(ResourceType r) const
{
	auto it = inventory.find(r);
	if (it == inventory.end())
		return 0;
	return it->second;
}
void ResourceManager::Add(ResourceType r, float amount)
{
	inventory[r] += amount;
}
bool ResourceManager::Request(ResourceType r, float amount)
{
	auto it = inventory.find(r);
	if (it == inventory.end())
		return false;
	if (it->second < amount)
		return false;
	it->second -= amount;
	return true;
}

// TransportManager
TransportManager::TransportManager(AIBrain* owner) : owner(owner) {}
void TransportManager::Update(float dt)
{
	(void)dt;
	if (pendingTransports > 0)
		pendingTransports = std::max(0, pendingTransports - 1);
}
void TransportManager::ScheduleTransport(ResourceType r, int amount, const Vec2& from, const Vec2& to)
{
	(void)r; (void)amount; (void)from; (void)to;
	pendingTransports++;
}

// BuildManager
BuildManager::BuildManager(AIBrain* owner) : owner(owner) {}
void BuildManager::Update(float dt)
{
	(void)dt;
	if (queue.empty()) return;
	std::string name = queue.front();
	if (name == "Barrack")
	{
		if (owner->GetResources()->Get(ResourceType::Wood) < BARRACK_WOOD_COST)
		{
			Task gather;
			gather.type = TaskType::FellTrees;
			gather.resource = ResourceType::Wood;
			gather.priority = 5;
			gather.amount = BARRACK_WOOD_COST;
			owner->GetAllocator()->AddTask(gather);
		}
		if (owner->GetResources()->Get(ResourceType::Iron) < BARRACK_IRON_COST)
		{
			Task gather;
			gather.type = TaskType::MineIron;
			gather.resource = ResourceType::Iron;
			gather.priority = 5;
			gather.amount = BARRACK_IRON_COST;
			owner->GetAllocator()->AddTask(gather);
		}

		if (owner->GetResources()->Get(ResourceType::Wood) >= BARRACK_WOOD_COST && owner->GetResources()->Get(ResourceType::Iron) >= BARRACK_IRON_COST)
		{
			owner->GetResources()->Request(ResourceType::Wood, BARRACK_WOOD_COST);
			owner->GetResources()->Request(ResourceType::Iron, BARRACK_IRON_COST);

			builtBuildings.push_back(name);
			queue.erase(queue.begin());
			Logger::Instance().Log(std::string("Built: ") + name + "\n");
			owner->GetAllocator()->RemoveTaskByMeta(name);
		}
	}
	else
	{
		builtBuildings.push_back(name);
		queue.erase(queue.begin());
	}
}

bool BuildManager::HasBuilding(const std::string& name) const
{
	return std::find(builtBuildings.begin(), builtBuildings.end(), name) != builtBuildings.end();
}
void BuildManager::QueueBuilding(const std::string& name, const Vec2& pos)
{
	(void)pos;
	queue.push_back(name);
}

// ManufacturingManager
ManufacturingManager::ManufacturingManager(AIBrain* owner) : owner(owner) {}
void ManufacturingManager::Update(float dt)
{
	(void)dt;
	for (auto it = orders.begin(); it != orders.end(); )
	{
		it->second = std::max(0, it->second - 1);
		if (it->second == 0)
			it = orders.erase(it);
		else ++it;
	}
}
void ManufacturingManager::QueueManufacture(const std::string& item, int amount)
{
	orders[item] += amount;
}

// MilitaryManager
MilitaryManager::MilitaryManager(AIBrain* owner) : owner(owner) {}
void MilitaryManager::Update(float dt)
{
	(void)dt;
	if (trainingQueue > 0)
	{
		trainingQueue = std::max(0, trainingQueue - 1);
		soldiers += 1;
		Logger::Instance().Log(std::string("Trained soldier. Total now: ") + std::to_string(soldiers) + "\n");
	}
}
void MilitaryManager::TrainSoldiers(int count) { trainingQueue += count; }

// TaskAllocator
TaskAllocator::TaskAllocator(AIBrain* owner) : owner(owner) {}
int TaskAllocator::AddTask(const Task& t)
{
	for (Task& existing : tasks)
	{
		if (!existing.assigned && existing.type == t.type && existing.resource == t.resource && existing.meta == t.meta)
		{
			existing.priority = std::max(existing.priority, t.priority);
			return existing.id;
		}
	}
	Task copy = t; copy.id = nextId++; tasks.push_back(copy); return copy.id;
}

void TaskAllocator::Update(float dt)
{
	(void)dt;
}
bool TaskAllocator::HasPending() const
{
	for (const Task& t : tasks)
		if (!t.assigned)
			return true;
	return false;
}

Task TaskAllocator::GetNext()
{
	auto it = std::max_element(tasks.begin(), tasks.end(), [](const Task& a, const Task& b)
		{
			return a.priority < b.priority;
		});
	if (it == tasks.end())
		return Task();
	Task t = *it;
	for (Task& tt : tasks)
	{
		if (tt.id == t.id)
		{
			tt.assigned = true;
			break;
		}
	}
	return t;
}

void TaskAllocator::RemoveTask(int id)
{
	tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [id](const Task& t) { return t.id == id; }), tasks.end());
}

void TaskAllocator::RemoveTaskByMeta(const std::string& meta)
{
	tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [meta](const Task& t) { return t.meta == meta; }), tasks.end());
}
