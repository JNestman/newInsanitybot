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

void GameCommander::onUnitDestroy(BWAPI::Unit unit)
{
	_informationManager.onUnitDestroy(unit);
	//_workerManager.onUnitDestroy(unit);
	//_unitManager.onUnitDestroy(unit);
}

void insanitybot::GameCommander::onUnitRenegade(BWAPI::Unit unit)
{
	//_informationManager.onUnitRenegade(unit);
	//_creationManager.checkQueue(unit, _informationManager.getQueue(), _informationManager.getAndAlterMinerals(), _informationManager.getAndAlterGas());
}

void insanitybot::GameCommander::onUnitComplete(BWAPI::Unit unit)
{
	//_informationManager.onUnitComplete(unit);
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
	Broodwar->drawTextScreen(50, 30, "bullyHunters: %d", _informationManager.getBullyHunters().size());
	Broodwar->drawTextScreen(50, 40, "isOneBasePlay: %d", _informationManager.isOneBasePlay(_informationManager.getStrategy()));
	Broodwar->drawTextScreen(50, 50, "isTwoBasePlay: %d", _informationManager.isTwoBasePlay(_informationManager.getStrategy()));
	Broodwar->drawTextScreen(50, 60, "isExpanding: %d", _informationManager.isExpanding());
	Broodwar->drawTextScreen(50, 70, "targetIsDefended: %d", _informationManager.targetIsDefended());
	//Broodwar->drawTextScreen(50, 80, "numFrontierSquadsWanted: %d", _informationManager.numFrontierSquadsNeeded());

	Broodwar->drawCircleMap(_informationManager.getMainChokePos(), 10, BWAPI::Colors::Orange);
	Broodwar->drawCircleMap(_informationManager.getNaturalChokePos(), 10, BWAPI::Colors::Red);
	Broodwar->drawCircleMap(BWAPI::Position(_informationManager.getNatPosition()), 20, BWAPI::Colors::Green);
	Broodwar->drawCircleMap(BWAPI::Position(_informationManager.getMainPosition()), 800, BWAPI::Colors::Cyan);
	
	for (auto base : _informationManager.getOwnedBases())
	{
		Broodwar->drawTextMap(base.first, "workers wanted: %d", base.second->numWorkersWantedHere());
		Broodwar->drawTextMap(base.first + BWAPI::Position(0, 10), "mineral here:   %d", base.second->getNumMineralWorkers());
		Broodwar->drawTextMap(base.first + BWAPI::Position(0, 20), "turrets here:   %d", base.second->numTurretsHere());

		if (base.second->baseHasRefinery())
		{
			Broodwar->drawTextMap(base.second->getRefineryPos(), "Gas: %d", base.second->getNumGasWorkers());
		}
		
		for (auto assignments : base.second->getRefineryAssignments())
		{
			if (assignments.second.size())
			{
				for (auto worker : assignments.second)
				{
					if (worker && worker->exists())
					{
						Broodwar->drawTextMap(worker->getPosition(), "Gas");
					}
				}
			}
		}
	}

	if (_informationManager.getEnemyBases().size())
	{
		BWAPI::TilePosition center = BWAPI::TilePosition(BWAPI::Broodwar->mapWidth() / 2, BWAPI::Broodwar->mapHeight() / 2);

		BWAPI::Position forwardPosition = BWAPI::Position(_informationManager.getEnemyBases().begin()->first.x + (BWAPI::Position(center).x - _informationManager.getEnemyBases().begin()->first.x) * .35,
			_informationManager.getEnemyBases().begin()->first.y + (BWAPI::Position(center).y - _informationManager.getEnemyBases().begin()->first.y) * .35);

		Broodwar->drawCircleMap(forwardPosition, 20, BWAPI::Colors::Green, true);

		BWAPI::Position nukeGather = BWAPI::Position(_informationManager.getEnemyNatChokePos().x + (BWAPI::Position(center).x - _informationManager.getEnemyNatChokePos().x) * .25,
			_informationManager.getEnemyNatChokePos().y + (BWAPI::Position(center).y - _informationManager.getEnemyNatChokePos().y) * .25);

		Broodwar->drawCircleMap(nukeGather, 25, BWAPI::Colors::Red, true);

		BWAPI::Position turretPositionStart = BWAPI::Position(BWAPI::Position(_informationManager.getMainPosition()).x + (BWAPI::Position(_informationManager.getEnemyMainTilePos()).x - BWAPI::Position(_informationManager.getMainPosition()).x) * .10,
			BWAPI::Position(_informationManager.getMainPosition()).y + (BWAPI::Position(_informationManager.getEnemyMainTilePos()).y - BWAPI::Position(_informationManager.getMainPosition()).y) * .10);

		Broodwar->drawCircleMap(turretPositionStart, 30, BWAPI::Colors::Cyan, true);
	}
}

GameCommander & GameCommander::Instance()
{
	static GameCommander instance;
	return instance;
}