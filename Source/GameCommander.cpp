#include "GameCommander.h"

using namespace insanitybot;


insanitybot::GameCommander::GameCommander()
{
}

void GameCommander::initialize()
{
	
}

void GameCommander::update()
{
	_informationManager.update();
	_workerManager.update(_informationManager.getWorkers(), _informationManager.getNumProducers());
	_unitManager.update();
	_creationManager.update(_informationManager.getProducers(), _informationManager.getCommandCenters(), _informationManager.getWorkers()
							, _informationManager.numWorkersWanted(), _informationManager.getQueue());
}

GameCommander & GameCommander::Instance()
{
	static GameCommander instance;
	return instance;
}