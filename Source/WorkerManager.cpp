#include "WorkerManager.h"
#include "InformationManager.h"

using namespace insanitybot;

void WorkerManager::initialize()
{
	_lastCheckSupply = 0;
	_lastCheckBuild = 0;
	_repairWorkers.clear();
	_mineralClearer = NULL;
}

void insanitybot::WorkerManager::update(InformationManager & _infoManager)
{
	std::map<BWAPI::Unit, BWEM::Base*>& _workers = _infoManager.getWorkers();
	std::vector<BWAPI::Unit> mineralsNeedClearing = _infoManager.mineralsNeedClear();
	bool needDefense = _infoManager.shouldHaveDefenseSquad(true);

	// If we have no workers, don't bother running any of this
	if (!_workers.size())
		return;

	/*****************************************************************************
	* Worker defense
	******************************************************************************/
	if (needDefense)
	{
		int numberOfEnemies = 0;
		BWAPI::Unitset foes;
		foes.clear();
		for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
		{
			if (!unit)
				continue;

			if (unit->exists() && unit->getDistance(BWAPI::Position(_infoManager.getMainPosition())) < 600 && !unit->isFlying())
			{
				foes.insert(unit);

				numberOfEnemies++;
			}
		}

		BWAPI::Unit target;

		if (_bullyHunters.size() && numberOfEnemies)
		{
			for (auto hunter : _bullyHunters)
			{
				int closest = 99999;
				target = *foes.begin();

				for (auto enemy : foes)
				{
					if (enemy->getDistance(hunter) < closest)
					{
						target = enemy;
						closest = enemy->getDistance(hunter);
					}
				}

				if (!hunter->isAttacking())
					hunter->attack(target);
			}
		}
		else if (_bullyHunters.size())
		{
			for (auto worker : _bullyHunters)
			{
				_workers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(worker, _infoManager.getOwnedBases().begin()->second));
			}

			_bullyHunters.clear();
		}
		
		if (numberOfEnemies && _workers.size() && _bullyHunters.size() <= numberOfEnemies * 2 )
		{
			assignBullyHunters(_workers, numberOfEnemies);
		}
	}
	else if (_bullyHunters.size())
	{
		for (auto worker : _bullyHunters)
		{
			_workers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(worker, _infoManager.getOwnedBases().begin()->second));
		}

		_bullyHunters.clear();
	}

	/*****************************************************************************
	* Building repair is handled here. Prioritise bunkers, then turrets, then general
	* repairs
	******************************************************************************/
	bool needRepair = false;

	// Repair for turrets
	for (auto turret : _infoManager.getTurrets())
	{
		if (turret->isAttacking() || turret->isUnderAttack() || turret->getHitPoints() < turret->getType().maxHitPoints())
		{
			needRepair = true;
			if (_repairWorkers.size() > 2)
			{
				for (auto & worker : _repairWorkers)
				{
					if (turret->getHitPoints() < turret->getType().maxHitPoints() && worker->isIdle())
					{
						worker->repair(turret);
					}
					else
					{
						worker->move(turret->getPosition());
					}
				}
			}
			else
			{
				assignRepairWorkers(_workers, turret);
			}
		}
	}

	// Repair for bunker
	for (auto bunker : _infoManager.getBunkers())
	{
		if (bunker->isAttacking() || bunker->isUnderAttack() || bunker->getHitPoints() < bunker->getType().maxHitPoints())
		{
			needRepair = true;
			if (_repairWorkers.size() > 2)
			{
				for (auto & worker : _repairWorkers)
				{
					if (bunker->getHitPoints() < bunker->getType().maxHitPoints())
					{
						worker->repair(bunker);
					}
					else
					{
						worker->move(bunker->getPosition());
					}
				}
			}
			else
			{
				assignRepairWorkers(_workers, bunker);
			}
		}
	}

	if (!needRepair && _infoManager.getInjuredBuildings().size())
	{
		for (auto building : _infoManager.getInjuredBuildings())
		{
			needRepair = true;
			if (_repairWorkers.size())
			{
				for (auto & worker : _repairWorkers)
				{
					if (building->getHitPoints() < building->getType().maxHitPoints())
					{
						worker->repair(building);
					}
				}
			}
			else
			{
				assignRepairWorkers(_workers, building);
			}
		}
	}
	

	// If we don't need to repair, clear the assigned workers
	if (!needRepair && _repairWorkers.size())
	{
		for (auto & worker : _repairWorkers)
		{
			if (worker && worker->exists() && _infoManager.getOwnedBases().size())
				_workers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(worker, _infoManager.getOwnedBases().begin()->second));
		}

		_repairWorkers.clear();
	}

	// Are we expanding to an island base?
	/*if (_infoManager.shouldIslandExpand())
	{
		if (!_infoManager.getIslandBuilder())
		{
			for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
			{
				if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
				{
					Broodwar << it->first->getType() << " found in worker list." << std::endl;
					continue;
				}

				if (it->first->getType().isWorker() && (it->first->isIdle() || it->first->isMoving() || it->first->isGatheringMinerals()) &&
					!it->first->isCarryingGas() && !it->first->isCarryingMinerals() && !it->second->isGasWorker(it->first) && !it->first->isConstructing())
				{
					_infoManager.setIslandBuilder(it->first);
					it->second->removeAssignment(it->first);
					_workers.erase(it);

					it->first->move(_infoManager.getOwnedBases().begin()->first);

					break;
				}
			}
		}
		else
		{
			BWAPI::Unit & builder = _infoManager.getIslandBuilder();

			for (auto mineral : _infoManager.getIslandSmallMinerals())
			{
				if (abs(builder->getPosition().x - mineral->getPosition().x) <= 300 && abs(builder->getPosition().y - mineral->getPosition().y) <= 300 && builder->isIdle())
				{
					builder->rightClick(mineral);
				}
			}

			if (builder->isCarryingMinerals())
			{
				int shortest = 9999999;
				BWAPI::TilePosition newBase;
				for (auto base : _infoManager.getIslandBases())
				{
					if (builder->getDistance(base.first) < shortest)
					{
						shortest = builder->getDistance(base.first);
						newBase = base.second->Location();
					}
				}

				builder->build(BWAPI::UnitTypes::Terran_Command_Center, newBase);
				if (_infoManager.getReservedMinerals() >= 400)
				{
					_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() - 400);
				}

				_infoManager.setIslandExpand(false);
			}
		}
	}
	else if (_infoManager.getIslandBuilder())
	{
		BWAPI::Unit & builder = _infoManager.getIslandBuilder();

		if (builder->isCarryingMinerals())
		{
			int shortest = 9999999;
			BWAPI::TilePosition newBase;
			for (auto base : _infoManager.getIslandBases())
			{
				if (builder->getDistance(base.first) < shortest)
				{
					shortest = builder->getDistance(base.first);
					newBase = base.second->Location();
				}
			}

			builder->build(BWAPI::UnitTypes::Terran_Command_Center, newBase);
		}
	}*/

	// Clear out blocking minerals that are close to our command centers.
	if (mineralsNeedClearing.size() && Broodwar->getFrameCount() > 10000)
	{
		if (_mineralClearer != NULL)
		{
			if (closeEnough(_mineralClearer->getPosition(), mineralsNeedClearing.front()->getPosition()) && !_mineralClearer->isGatheringMinerals())
				_mineralClearer->rightClick(mineralsNeedClearing.front());
			else if (!closeEnough(_mineralClearer->getPosition(), mineralsNeedClearing.front()->getPosition()))
				_mineralClearer->move(mineralsNeedClearing.front()->getPosition());
		}
		else
		{
			assignMineralClearer(_workers, mineralsNeedClearing);
		}
	}
	else if (_mineralClearer != NULL)
	{
		_workers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(_mineralClearer, _infoManager.getOwnedBases().begin()->second));
		_mineralClearer = NULL;
	}
	
	// Check each worker's assignment (i.e. gathering minerals/gas)
	for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
	{
		if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
		{
			Broodwar << it->first->getType() << " found in worker list." << std::endl;
			_workers.erase(it);
			continue;
		}

		// Redundant check to make sure all workers are assigned to non-destroyed bases
		bool found = false;
		for (auto base : _infoManager.getOwnedBases())
		{
			if (it->second == base.second)
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			// ToDo: We still don't have a good workaround for reassigning workers with no owned bases. Will crash the bot.
			// Though, at that point, the chances of winning would be slim.
			if (_infoManager.getOwnedBases().size())
				it->second = _infoManager.getOwnedBases().begin()->second;
		}

		if (it->first->isIdle() || (it->second->baseHasRefinery() && it->second->getNumGasWorkers() < 3) ||
			(it->first->isGatheringMinerals() && !it->second->miningAssignmentExists(it->first)))
		{
			it->second->checkAssignment(it->first, _infoManager.getOwnedBases(), it->second);
		}
	}
	for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _infoManager.getIslandWorkers().begin(); it != _infoManager.getIslandWorkers().end(); it++)
	{
		if (it->first->isIdle() || (it->second->baseHasRefinery() && it->second->getNumGasWorkers() < 3) ||
			(it->first->isGatheringMinerals() && !it->second->miningAssignmentExists(it->first)))
		{
			it->second->checkAssignment(it->first, _infoManager.getOwnedIslandBases(), it->second);
		}
	}

	// If a building has lost its worker, assign a new one
	for (auto & building : _infoManager.getConstructing())
	{
		if (building->isUnderAttack() && building->getHitPoints() < 30)
		{
			building->cancelConstruction();
			continue;
		}

		if (building->getBuildUnit() == NULL || !building->getBuildUnit()->exists())
		{
			if (_workers.size())
			{
				int shortestDistance = 999999;
				std::map<BWAPI::Unit, BWEM::Base *>::iterator & builder = _workers.begin();

				for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
				{
					if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
					{
						Broodwar << it->first->getType() << " found in worker list." << std::endl;
						continue;
					}

					// Check for closest available worker that is on the same heigh level as our destination and is not too far away
					if (it->first->getType() == building->getType().whatBuilds().first && (it->first->isIdle() || it->first->isMoving() || it->first->isGatheringMinerals())
						&& !it->first->isCarryingGas() && !it->second->isGasWorker(it->first) && !it->first->isConstructing() &&
						it->first->getDistance(building->getPosition()) < shortestDistance && 
						it->first->getDistance(building->getPosition()) < 1200 &&
						BWAPI::Broodwar->getGroundHeight(it->first->getTilePosition()) == BWAPI::Broodwar->getGroundHeight(building->getTilePosition()))
					{
						shortestDistance = it->first->getDistance(building->getPosition());
						builder = it;
					}
				}

				if (builder->first)
				{
					builder->first->rightClick(building);
					builder->second->removeAssignment(builder->first);
				}
				else
				{
					// If we didn't find a builder, remove the height and distance requirements
					for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
					{
						if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
						{
							Broodwar << it->first->getType() << " found in worker list." << std::endl;
							continue;
						}

						if (it->first->getType() == building->getType().whatBuilds().first && (it->first->isIdle() || it->first->isMoving() || it->first->isGatheringMinerals())
							&& !it->first->isCarryingGas() && !it->second->isGasWorker(it->first) && !it->first->isConstructing() &&
							it->first->getDistance(building->getPosition()) < shortestDistance)
						{
							shortestDistance = it->first->getDistance(building->getPosition());
							builder = it;
						}
					}

					if (builder->first)
					{
						builder->first->rightClick(building);
						builder->second->removeAssignment(builder->first);
					}
				}
			}
		}
		else if (!building->getBuildUnit()->isConstructing())
		{
			BWAPI::Unit builder = building->getBuildUnit();

			builder->rightClick(building);
		}
	}
}

void WorkerManager::construct(std::map<BWAPI::Unit, BWEM::Base *>& _workers, BWAPI::UnitType structure, BWAPI::TilePosition targetLocation)
{
	if (_workers.size())
	{
		int shortestDistance = 999999;
		std::map<BWAPI::Unit, BWEM::Base *>::iterator & builder = _workers.begin();

		for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
		{
			if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
			{
				Broodwar << it->first->getType() << " found in worker list." << std::endl;
				continue;
			}

			// Check for closest available worker that is on the same heigh level as our destination and is not too far away
			if (it->first->getType() == structure.whatBuilds().first && (it->first->isIdle() || it->first->isMoving() || it->first->isGatheringMinerals())
				&& !it->first->isCarryingGas() && !it->second->isGasWorker(it->first) && !it->first->isConstructing() &&
				it->first->getDistance(BWAPI::Position(targetLocation)) < shortestDistance &&
				it->first->getDistance(BWAPI::Position(targetLocation)) < 1200 &&
				BWAPI::Broodwar->getGroundHeight(it->first->getTilePosition()) == BWAPI::Broodwar->getGroundHeight(targetLocation))
			{
				shortestDistance = it->first->getDistance(BWAPI::Position(targetLocation));
				builder = it;
			}
		}

		if (builder != _workers.begin())
		{
			// Register an event that draws the target build location
			Broodwar->registerEvent([targetLocation, structure](Game*)
			{
				Broodwar->drawBoxMap(Position(targetLocation),
					Position(targetLocation + structure.tileSize()),
					Colors::Blue);
			},
				nullptr,  // condition
				structure.buildTime() + 100);  // frames to run

			bool targetVisible = true;

			for (int x = 0; x < structure.tileWidth(); ++x)
			{
				for (int y = 0; y < structure.tileHeight(); ++y)
				{
					if (!BWAPI::Broodwar->isExplored(targetLocation.x + x, targetLocation.y + y))
					{
						targetVisible = false;
					}
				}
			}
			if (targetVisible)
			{
				builder->first->build(structure, targetLocation);
				builder->second->removeAssignment(builder->first);
			}
			else
			{
				builder->first->move(BWAPI::Position(targetLocation));
				builder->second->removeAssignment(builder->first);
			}
		}
		else
		{
			// If we didn't find a builder, remove the height and distance requirements
			for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
			{
				if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
				{
					Broodwar << it->first->getType() << " found in worker list." << std::endl;
					continue;
				}

				if (it->first->getType() == structure.whatBuilds().first && (it->first->isIdle() || it->first->isMoving() || it->first->isGatheringMinerals())
					&& !it->first->isCarryingGas() && !it->second->isGasWorker(it->first) && !it->first->isConstructing() &&
					it->first->getDistance(BWAPI::Position(targetLocation)) < shortestDistance)
				{
					shortestDistance = it->first->getDistance(BWAPI::Position(targetLocation));
					builder = it;
				}
			}

			if (builder->first)
			{
				// Register an event that draws the target build location
				Broodwar->registerEvent([targetLocation, structure](Game*)
				{
					Broodwar->drawBoxMap(Position(targetLocation),
						Position(targetLocation + structure.tileSize()),
						Colors::Blue);
				},
					nullptr,  // condition
					structure.buildTime() + 100);  // frames to run

				bool targetVisible = true;

				for (int x = 0; x < structure.tileWidth(); ++x)
				{
					for (int y = 0; y < structure.tileHeight(); ++y)
					{
						if (!BWAPI::Broodwar->isExplored(targetLocation.x + x, targetLocation.y + y))
						{
							targetVisible = false;
						}
					}
				}
				if (targetVisible)
				{
					builder->first->build(structure, targetLocation);
					builder->second->removeAssignment(builder->first);
				}
				else
				{
					builder->first->move(BWAPI::Position(targetLocation));
					builder->second->removeAssignment(builder->first);
				}
			}
		}
	}
}

void WorkerManager::supplyConstruction(std::map<BWAPI::Unit, BWEM::Base *>& _workers, BWAPI::TilePosition targetBuildLocation, int reservedMinerals)
{
	//////////////////////////////////////////////////////////////////////////////
	// Supply Production
	//////////////////////////////////////////////////////////////////////////////
	UnitType supplyProviderType = BWAPI::UnitTypes::Terran_Supply_Depot;

	// If we are supply blocked and haven't tried constructing more recently
	if (_workers.size())
	{
		int shortestDistance = 999999;
		std::map<BWAPI::Unit, BWEM::Base *>::iterator & builder = _workers.begin();

		for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
		{
			if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
			{
				Broodwar << it->first->getType() << " found in worker list." << std::endl;
				continue;
			}

			// Check for closest available worker that is on the same heigh level as our destination and is not too far away
			if (it->first->getType() == supplyProviderType.whatBuilds().first && (it->first->isIdle() || it->first->isMoving() || it->first->isGatheringMinerals())
				&& !it->first->isCarryingGas() && !it->second->isGasWorker(it->first) && !it->first->isConstructing() &&
				it->first->getDistance(BWAPI::Position(targetBuildLocation)) < shortestDistance &&
				it->first->getDistance(BWAPI::Position(targetBuildLocation)) < 1200 &&
				BWAPI::Broodwar->getGroundHeight(it->first->getTilePosition()) == BWAPI::Broodwar->getGroundHeight(targetBuildLocation))
			{
				shortestDistance = it->first->getDistance(BWAPI::Position(targetBuildLocation));
				builder = it;
			}
		}

		if (builder->first)
		{
			// Register an event that draws the target build location
			Broodwar->registerEvent([targetBuildLocation, supplyProviderType](Game*)
			{
				Broodwar->drawBoxMap(Position(targetBuildLocation),
					Position(targetBuildLocation + supplyProviderType.tileSize()),
					Colors::Blue);
			},
				nullptr,  // condition
				supplyProviderType.buildTime() + 100);  // frames to run

			bool targetVisible = true;

			for (int x = 0; x < supplyProviderType.tileWidth(); ++x)
			{
				for (int y = 0; y < supplyProviderType.tileHeight(); ++y)
				{
					if (!BWAPI::Broodwar->isExplored(targetBuildLocation.x + x, targetBuildLocation.y + y))
					{
						targetVisible = false;
					}
				}
			}
			if (targetVisible)
			{
				builder->first->build(supplyProviderType, targetBuildLocation);
				builder->second->removeAssignment(builder->first);
			}
			else
			{
				builder->first->move(BWAPI::Position(targetBuildLocation));
				builder->second->removeAssignment(builder->first);
			}
		}
		else
		{
			// If we didn't find a builder, remove the height and distance requirements
			for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
			{
				if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
				{
					Broodwar << it->first->getType() << " found in worker list." << std::endl;
					continue;
				}

				if (it->first->getType() == supplyProviderType.whatBuilds().first && (it->first->isIdle() || it->first->isMoving() || it->first->isGatheringMinerals())
					&& !it->first->isCarryingGas() && !it->second->isGasWorker(it->first) && !it->first->isConstructing() &&
					it->first->getDistance(BWAPI::Position(targetBuildLocation)) < shortestDistance)
				{
					shortestDistance = it->first->getDistance(BWAPI::Position(targetBuildLocation));
					builder = it;
				}
			}

			if (builder->first)
			{
				// Register an event that draws the target build location
				Broodwar->registerEvent([targetBuildLocation, supplyProviderType](Game*)
				{
					Broodwar->drawBoxMap(Position(targetBuildLocation),
						Position(targetBuildLocation + supplyProviderType.tileSize()),
						Colors::Blue);
				},
					nullptr,  // condition
					supplyProviderType.buildTime() + 100);  // frames to run

				bool targetVisible = true;

				for (int x = 0; x < supplyProviderType.tileWidth(); ++x)
				{
					for (int y = 0; y < supplyProviderType.tileHeight(); ++y)
					{
						if (!BWAPI::Broodwar->isExplored(targetBuildLocation.x + x, targetBuildLocation.y + y))
						{
							targetVisible = false;
						}
					}
				}
				if (targetVisible)
				{
					builder->first->build(supplyProviderType, targetBuildLocation);
					builder->second->removeAssignment(builder->first);
				}
				else
				{
					builder->first->move(BWAPI::Position(targetBuildLocation));
					builder->second->removeAssignment(builder->first);
				}
			}
		}
	}
}

void insanitybot::WorkerManager::assignBullyHunters(std::map<BWAPI::Unit, BWEM::Base*>& _workers, int numberOfEnemies)
{
	if (_workers.size())
	{
		std::map<BWAPI::Unit, BWEM::Base *>::iterator & hunter = _workers.begin();

		if (hunter->first->getType() != BWAPI::UnitTypes::Terran_SCV)
		{
			Broodwar << hunter->first->getType() << " found in worker list." << std::endl;
			return;
		}

		if (hunter->first)
		{
			_bullyHunters.push_back(hunter->first);
			hunter->second->removeAssignment(hunter->first);
			_workers.erase(hunter);
		}
	}

	if (_bullyHunters.size() < numberOfEnemies * 2 && _workers.size())
	{
		assignBullyHunters(_workers, numberOfEnemies);
	}
}

void insanitybot::WorkerManager::assignRepairWorkers(std::map<BWAPI::Unit, BWEM::Base *>& _workers, BWAPI::Unit building)
{
	if (_workers.size())
	{
		int shortestDistance = 999999;
		std::map<BWAPI::Unit, BWEM::Base *>::iterator & repairer = _workers.begin();

		for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
		{
			if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
			{
				Broodwar << it->first->getType() << " found in worker list." << std::endl;
				continue;
			}

			if (it->first->getType().isWorker() && !it->first->isConstructing() &&
				it->first->getDistance(building->getPosition()) < shortestDistance)
			{
				shortestDistance = it->first->getDistance(building->getPosition());
				repairer = it;
			}
		}

		if (repairer->first)
		{
			_repairWorkers.push_back(repairer->first);
			repairer->second->removeAssignment(repairer->first);
			_workers.erase(repairer);
		}
	}
	else
	{
		return;
	}

	if ((building->getType() == BWAPI::UnitTypes::Terran_Bunker || building->getType() == BWAPI::UnitTypes::Terran_Missile_Turret) && _repairWorkers.size() < 3)
	{
		assignRepairWorkers(_workers, building);
	}
}

void insanitybot::WorkerManager::assignMineralClearer(std::map<BWAPI::Unit, BWEM::Base*>& _workers, std::vector<BWAPI::Unit> needClearing)
{
	if (_workers.size() && needClearing.size())
	{
		int shortestDistance = 999999;
		std::map<BWAPI::Unit, BWEM::Base *>::iterator & clearer = _workers.begin();

		for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
		{
			if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
			{
				Broodwar << it->first->getType() << " found in worker list." << std::endl;
				continue;
			}

			if (it->first->getType().isWorker() && !it->first->isConstructing() && !it->first->isCarryingMinerals() &&
				it->first->getDistance(needClearing.front()->getPosition()) < shortestDistance)
			{
				shortestDistance = it->first->getDistance(needClearing.front()->getPosition());
				clearer = it;
			}
		}

		if (clearer->first)
		{
			_mineralClearer = clearer->first;
			clearer->second->removeAssignment(clearer->first);
			_workers.erase(clearer);
		}
	}
}

bool insanitybot::WorkerManager::checkSupplyConstruction(int numProducers, int reservedMinerals)
{
	int moreSupply = numProducers / 3;

	if (moreSupply >= 4)
		moreSupply = 4;

	if (BWAPI::Broodwar->self()->supplyUsed() + 8 >= BWAPI::Broodwar->self()->supplyTotal() &&
		_lastCheckSupply + 300 < Broodwar->getFrameCount() &&
		Broodwar->self()->incompleteUnitCount(BWAPI::UnitTypes::Terran_Supply_Depot) <= 0 + moreSupply &&
		Broodwar->self()->minerals() - reservedMinerals > BWAPI::UnitTypes::Terran_Supply_Depot.mineralPrice() &&
		Broodwar->self()->supplyTotal() < 400)
	{
		_lastCheckSupply = Broodwar->getFrameCount();
		return true;
	}
	else
		return false;
}

bool insanitybot::WorkerManager::closeEnough(BWAPI::Position location1, BWAPI::Position location2)
{
	// If the coordinates are "close enough", we call it good.
	return abs(location1.x - location2.x) <= 128 && abs(location1.y - location2.y) <= 128;
}

void WorkerManager::onUnitDestroy(BWAPI::Unit unit)
{
	if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self())
	{
		if (unit == _mineralClearer)
		{
			_mineralClearer = NULL;
			return;
		}
		else if (_repairWorkers.size() || _bullyHunters.size())
		{
			for (auto worker : _repairWorkers)
			{
				if (worker == unit)
				{
					_repairWorkers.remove(worker);
					return;
				}
			}

			for (auto worker : _bullyHunters)
			{
				if (worker == unit)
				{
					_bullyHunters.remove(worker);
					return;
				}
			}
		}
	}
}

void insanitybot::WorkerManager::infoText()
{
	Broodwar->drawTextScreen(50, 20, "repairWorkers.size(): %d", _repairWorkers.size());
}

WorkerManager & WorkerManager::Instance()
{
	static WorkerManager instance;
	return instance;
}