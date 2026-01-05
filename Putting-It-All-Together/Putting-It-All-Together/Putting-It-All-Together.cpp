#include <iostream>
#include "Constants.h"
#include "AStar.h"
#include "GameAI.h"
#include "GameLoop.h"
#include "Grid.h"
#include "Logger.h"
#include "Movable.h"
#include "Pathfinder.h"
#include "PathNode.h"
#include "Player.h"
#include "Renderer.h"
#include "Vec2.h"
#include "random.h"

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>


int main()
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetBreakAlloc();

    Logger::Instance();

    Logger::Instance().Log("Program started!");

    // run 10 seconds at 60 FPS for demo; use -1.0 to run until closed
    GameLoop::Instance().RunGameLoop(-10.0, 120);

    return 0;
}
