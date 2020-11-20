#include "GameCommander.h"

using namespace insanitybot;


insanitybot::GameCommander::GameCommander()
{
}

void GameCommander::initialize()
{
	_informationManager.initialize();
	_creationManager.initialize();
	_workerManager.initialize();
}

void GameCommander::update()
{
	_informationManager.update();
	_workerManager.update(_informationManager);
	_unitManager.update(_informationManager);
	_creationManager.update(_informationManager);
}

void GameCommander::onUnitShow(BWAPI::Unit unit)
{
	_informationManager.onUnitShow(unit);
}

void GameCommander::onUnitCreate(BWAPI::Unit unit)
{
	_informationManager.onUnitCreate(unit);
	_creationManager.checkQueue(unit, _informationManager.getQueue(), _informationManager.getAndAlterMinerals(), _informationManager.getAndAlterGas());
}

void GameCommander::onUnitDestroy(BWAPI::Unit unit)
{
	_informationManager.onUnitDestroy(unit);
	_workerManager.onUnitDestroy(unit);
	_unitManager.onUnitDestroy(unit);
}

void insanitybot::GameCommander::onUnitRenegade(BWAPI::Unit unit)
{
	_informationManager.onUnitRenegade(unit);
	_creationManager.checkQueue(unit, _informationManager.getQueue(), _informationManager.getAndAlterMinerals(), _informationManager.getAndAlterGas());
}

void insanitybot::GameCommander::onUnitComplete(BWAPI::Unit unit)
{
	_informationManager.onUnitComplete(unit);
}

void GameCommander::infoText()
{
	Broodwar->drawTextScreen(200, 20, "Workers Wanted: %d", _informationManager.numWorkersWanted());
	Broodwar->drawTextScreen(200, 30, "numWorkers: %d", _informationManager.getNumWorkersOwned());
	Broodwar->drawTextScreen(200, 40, "Reserved: M %d, G %d", _informationManager.getReservedMinerals(), _informationManager.getReservedGas());
	Broodwar->drawTextScreen(200, 50, "OwnedBases: %d", _informationManager.getOwnedBases().size());
	Broodwar->drawTextScreen(200, 60, "islandBases: %d", _informationManager.getIslandBases().size());
	Broodwar->drawTextScreen(200, 70, "ownedIslandBases: %d", _informationManager.getOwnedIslandBases().size());
	Broodwar->drawTextScreen(200, 80, "EnemyBases: %d", _informationManager.getEnemyBases().size());
	Broodwar->drawTextScreen(200, 90, "EnemyStructures: %d", _informationManager.getEnemyBuildingPositions().size());
	_unitManager.infoText();

	Broodwar->drawCircleMap(_informationManager.getMainChokePos(), 10, BWAPI::Colors::Orange);
	Broodwar->drawCircleMap(_informationManager.getNaturalChokePos(), 10, BWAPI::Colors::Red);
	Broodwar->drawCircleMap(BWAPI::Position(_informationManager.getNatPosition()), 20, BWAPI::Colors::Green);
	Broodwar->drawCircleMap(BWAPI::Position(_informationManager.getMainPosition()), 800, BWAPI::Colors::Cyan);
	Broodwar->drawCircleMap(BWAPI::Position((BWAPI::Position(_informationManager.getMainPosition()).x + _informationManager.getMainChokePos().x) / 2, (BWAPI::Position(_informationManager.getMainPosition()).y + _informationManager.getMainChokePos().y) / 2), 10, BWAPI::Colors::Blue);
	
	BWAPI::TilePosition center = BWAPI::TilePosition(BWAPI::TilePosition(BWAPI::Broodwar->mapWidth() / 2, BWAPI::Broodwar->mapHeight() / 2));
	/*if (_informationManager.getEnemyBases().size())
	{
		if (BWAPI::Broodwar->mapHash() == "e47775e171fe3f67cc2946825f00a6993b5a415e") // La Mancha
		{
			Broodwar->drawCircleMap(BWAPI::Position((_informationManager.getEnemyNaturalPos().x * .70) + (BWAPI::Position(center).x * .30), (_informationManager.getEnemyNaturalPos().y * .70) + (BWAPI::Position(center).y * .30)), 40, BWAPI::Colors::Purple);
		}
		else
		{
			Broodwar->drawCircleMap(BWAPI::Position((_informationManager.getEnemyNaturalPos().x + BWAPI::Position(center).x) / 2, (_informationManager.getEnemyNaturalPos().y + BWAPI::Position(center).y) / 2), 40, BWAPI::Colors::Purple);
		}
	}*/
}

GameCommander & GameCommander::Instance()
{
	static GameCommander instance;
	return instance;
}