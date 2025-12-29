#pragma once
#include "GameAI.h"

class AIBrain
{
public:

	AIBrain(GameAI* owner) : ownerAI(owner)
	{
	}

	~AIBrain() {}

	void Think(float deltaTime);

	void UpdateValues(float deltaTime);

	void Decay(float deltaTime);

	void CheckDeath();

	void FSM(float deltaTime);

private:

	GameAI* ownerAI;

};