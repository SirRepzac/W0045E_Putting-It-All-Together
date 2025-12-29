#include "AIBrain.h"
#include "GameLoop.h"

void AIBrain::Think(float deltaTime)
{
	if (!ownerAI)
		return;
	// Simple need decay over time
	Decay(deltaTime);

	UpdateValues(deltaTime);

	FSM(deltaTime);

	CheckDeath();
}

void AIBrain::UpdateValues(float deltaTime)
{
	float allowedRange = (ownerAI->GetRadius() + DEFAULT_CELL_SIZE);
	Vec2 pos = ownerAI->GetPosition();
	Grid& grid = GameLoop::Instance().GetGrid();

}

// Decay of the AI's needs over time
void AIBrain::Decay(float deltaTime)
{

}

void AIBrain::CheckDeath()
{
	bool someCondition = false;
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

}
