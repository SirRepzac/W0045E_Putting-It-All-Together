#pragma once
#include <vector>
#include <functional>
#include "Behaviour.h"
#include "GameAI.h"
#include "GameLoop.h"
#include "Constants.h"
#include "Logger.h"
#include "Renderer.h"
#include "Player.h"
#include "Grid.h"
#include "Pathfinder.h"
#include "AIBrain.h"


class GameLoop
{
public:
	static GameLoop& Instance()
	{
		static GameLoop instance_;
		return instance_;
	}
	void operator=(const GameLoop&) = delete;

	GameLoop(GameLoop& other) = delete;
	~GameLoop();

	void DeleteAll();

	// Runs a loop that invokes perFrame(deltaSeconds) each frame (if provided).
	void RunGameLoop(double durationSeconds = -1.0, unsigned int fps = 60, std::function<void(float)> perFrame = nullptr);
	void InitializeGame();
	void SaveData();
	void LoadGameData();
	void UpdateGameLoop(float delta, double timePassed);

	void AddDebugEntity(Vec2 pos, uint32_t color = Renderer::Color(200, 0, 0)/*red*/, int radius = 1, bool filled = true, std::string name = "");
	void AddDebugLine(Vec2 a, Vec2 b, uint32_t color, float thickness = 2.0f);

	void AddDrawEntity(Renderer::Entity e) { environmentEnts.push_back(e); }

	std::vector<Movable*> GetMovables();
	std::vector<Renderer::Entity>& GetDebugEntities() { return debugEnts; }
	Grid& GetGrid() { return grid; }

	bool DEBUG_MODE = true;

	Pathfinder* pathfinder;

	void ScheduleDeath(GameAI* ai) { deathRow.push_back(ai); }

	void ClearDrawEntities() { environmentEnts.clear(); }
private:
	GameLoop();
	void ClearDebugEntities() { debugEnts.clear(); }
	void UpdateRenderer();
	void HandleInput(float delta);
	void CreatePlayer(Vec2 pos = Vec2(WORLD_WIDTH / 2.0f, WORLD_HEIGHT - 100.0f));
	void LMBMouseClickAction(Vec2 clickPos);
	void RMBMouseClickAction(Vec2 clickPos);
	void CreateAI(int count);
	void HandlePlayerInput(float delta);
	void ExecuteDeathRow();

	float keyPressCooldown = 0.0f;
	PathNode::Type currentPlacingType = PathNode::Wall;

	float gameSpeed = 1.0f;

	std::vector<Renderer::Entity> debugEnts;
	std::vector<Renderer::Entity> environmentEnts;
	std::vector<Renderer::Entity> persistentEnts;
	Grid grid;

	Renderer* renderer;
	Player* player = nullptr;

	std::vector<GameAI*> aiList;
	std::vector<GameAI*> deathRow;

	std::string dataPath = "Data";
	std::string positionPath = "wall_positions.data";
};
