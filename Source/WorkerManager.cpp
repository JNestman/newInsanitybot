#include "WorkerManager.h"
#include "InformationManager.h"

using namespace insanitybot;

void WorkerManager::initialize()
{
	_lastCheckSupply = 0;
	_lastCheckBuild = 0;
	_mineralClearer = NULL;
}

void insanitybot::WorkerManager::update(InformationManager & _infoManager)
{
	std::map<BWAPI::Unit, BWEM::Base*>& _workers = _infoManager.getWorkers();
	std::list<BWAPI::Unit>& _bullyHunters = _infoManager.getBullyHunters();
	std::list<BWAPI::Unit>& _repairWorkers = _infoManager.getRepairWorkers();
	std::vector<BWAPI::Unit> mineralsNeedClearing = _infoManager.mineralsNeedClear();
	std::map<BWAPI::Position, BWEM::Base *>	_ownedBases = _infoManager.getOwnedBases();
	bool needDefense = _infoManager.shouldHaveDefenseSquad(true);

	gasCutOff = false;

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
			if (!unit || !unit->exists())
				continue;

			if (unit->getDistance(BWAPI::Position(_infoManager.getMainPosition())) < 600 && !unit->isFlying())
			{
				foes.insert(unit);

				numberOfEnemies++;
			}
		}

		BWAPI::Unit target;

		if (_bullyHunters.size() && numberOfEnemies)
		{
			for (std::list<BWAPI::Unit>::iterator deadHunter = _bullyHunters.begin(); deadHunter != _bullyHunters.end();)
			{
				if (!(*deadHunter) || !(*deadHunter)->exists())
				{
					deadHunter = _bullyHunters.erase(deadHunter);
				}
				else
				{
					deadHunter++;
				}
			}

			for (auto & hunter : _bullyHunters)
			{
				if (!hunter || !hunter->exists())
					continue;

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

				if (!hunter->isAttacking() && !hunter->getGroundWeaponCooldown())
					hunter->attack(target);
			}
		}

		if (numberOfEnemies && _workers.size() && _bullyHunters.size() < numberOfEnemies * 2)
		{
			assignBullyHunters(_workers, _bullyHunters, numberOfEnemies, _ownedBases);
		}
		else if (!numberOfEnemies)
		{
			for (auto hunter : _bullyHunters)
			{
				if (hunter && hunter->exists())
				{
					hunter->stop();
				}
			}
			_bullyHunters.clear();
		}
	}
	else if (_bullyHunters.size())
	{
		_bullyHunters.clear();
	}

	/*****************************************************************************
	* Building repair is handled here. Prioritise bunkers, then turrets, then general
	* repairs
	******************************************************************************/
	bool needRepair = false;

	// Repair for bunker
	for (auto bunker : _infoManager.getBunkers())
	{
		if (!bunker || !bunker->exists())
		{
			continue;
		}

		if (!bunker->isBeingConstructed() && (_infoManager.getOwnedBases().size() < 2 || 
			(_infoManager.getOwnedBases().size() == 2 && _infoManager.getNumUnfinishedUnit(BWAPI::UnitTypes::Terran_Command_Center))) &&
			_infoManager.isTwoBasePlay(_infoManager.getStrategy()))
		{
			needRepair = true;
			if (_repairWorkers.size() > 2)
			{
				for (auto & worker : _repairWorkers)
				{
					if (!worker || !worker->exists())
					{
						for (std::list<BWAPI::Unit>::iterator deadRepairer = _repairWorkers.begin(); deadRepairer != _repairWorkers.end();)
						{
							if ((*deadRepairer) || !(*deadRepairer)->exists())
							{
								deadRepairer = _repairWorkers.erase(deadRepairer);
							}
							else
							{
								deadRepairer++;
							}
						}

						break;
					}

					if (bunker->getHitPoints() < bunker->getType().maxHitPoints())
					{
						if (!worker->isRepairing() || worker->getTarget() != bunker)
							worker->repair(bunker);
					}
					else
					{
						if (!_infoManager.closeEnough(worker->getPosition(), bunker->getPosition()))
							worker->move(bunker->getPosition());
					}
				}
			}
			else
			{
				assignRepairWorkers(_workers, _repairWorkers, bunker, _ownedBases);
			}
		}
		if (bunker->isAttacking() || bunker->isUnderAttack() || (bunker->getHitPoints() < bunker->getType().maxHitPoints() && !bunker->isBeingConstructed()))
		{
			needRepair = true;
			if (_repairWorkers.size() > 2)
			{
				for (auto & worker : _repairWorkers)
				{
					if (!worker || !worker->exists())
					{
						for (std::list<BWAPI::Unit>::iterator deadRepairer = _repairWorkers.begin(); deadRepairer != _repairWorkers.end();)
						{
							if ((*deadRepairer) || !(*deadRepairer)->exists())
							{
								deadRepairer = _repairWorkers.erase(deadRepairer);
							}
							else
							{
								deadRepairer++;
							}
						}

						break;
					}

					if (bunker->getHitPoints() < bunker->getType().maxHitPoints())
					{
						if (!worker->isRepairing() || worker->getTarget() != bunker)
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
				assignRepairWorkers(_workers, _repairWorkers, bunker, _ownedBases);
			}
		}
	}

	// Repair for turrets
	if (!needRepair)
	{
		for (auto turret : _infoManager.getTurrets())
		{
			if (!turret || !turret->exists())
			{
				continue;
			}

			if (turret->isAttacking() || turret->isUnderAttack() || (turret->getHitPoints() < turret->getType().maxHitPoints() && !turret->isBeingConstructed()))
			{
				needRepair = true;
				if (_repairWorkers.size() > 2)
				{
					for (auto & worker : _repairWorkers)
					{
						if (!worker || !worker->exists())
						{
							for (std::list<BWAPI::Unit>::iterator deadRepairer = _repairWorkers.begin(); deadRepairer != _repairWorkers.end();)
							{
								if ((*deadRepairer) || !(*deadRepairer)->exists())
								{
									deadRepairer = _repairWorkers.erase(deadRepairer);
								}
								else
								{
									deadRepairer++;
								}
							}

							break;
						}

						if (!worker->isRepairing() || worker->getTarget() != turret)
							worker->repair(turret);
					}
				}
				else
				{
					assignRepairWorkers(_workers, _repairWorkers, turret, _ownedBases);
				}
			}
		}
	}

	if (!needRepair && _infoManager.getInjuredBuildings().size())
	{
		for (auto building : _infoManager.getInjuredBuildings())
		{
			if (!building || !building->exists())
			{
				continue;
			}

			needRepair = true;
			if (_repairWorkers.size())
			{
				for (auto & worker : _repairWorkers)
				{
					if (!worker || !worker->exists())
					{
						for (std::list<BWAPI::Unit>::iterator deadRepairer = _repairWorkers.begin(); deadRepairer != _repairWorkers.end();)
						{
							if ((*deadRepairer) || !(*deadRepairer)->exists())
							{
								deadRepairer = _repairWorkers.erase(deadRepairer);
							}
							else
							{
								deadRepairer++;
							}
						}

						break;
					}

					if (building->getHitPoints() < building->getType().maxHitPoints())
					{
						if (!worker->isRepairing())
							worker->repair(building);
					}
				}
			}
			else
			{
				assignRepairWorkers(_workers, _repairWorkers, building, _ownedBases);
			}
		}
	}

	if (!needRepair && _infoManager.getDropships().size())
	{
		std::list<BWAPI::Unit> injuredDropships;
		injuredDropships.clear();
		for (auto dropship : _infoManager.getDropships())
		{
			if (!dropship.first || !dropship.first->exists())
				continue;

			if (dropship.first->getHitPoints() < dropship.first->getType().maxHitPoints() &&
				dropship.first->getDistance(BWAPI::Position(_infoManager.getMainPosition())) < 600)
			{
				needRepair = true;

				injuredDropships.push_back(dropship.first);
			}
		}

		for (auto dropship : injuredDropships)
		{
			needRepair = true;

			if (_repairWorkers.size())
			{
				for (auto & worker : _repairWorkers)
				{
					if (!worker || !worker->exists())
					{
						for (std::list<BWAPI::Unit>::iterator deadRepairer = _repairWorkers.begin(); deadRepairer != _repairWorkers.end();)
						{
							if ((*deadRepairer) || !(*deadRepairer)->exists())
							{
								deadRepairer = _repairWorkers.erase(deadRepairer);
							}
							else
							{
								deadRepairer++;
							}
						}

						break;
					}

					if (!worker->isRepairing())
						worker->repair(dropship);
				}
			}
			else
			{
				assignRepairWorkers(_workers, _repairWorkers, dropship, _ownedBases);
			}

			break;
		}
	}
	

	// If we don't need to repair, clear the assigned workers
	if (!needRepair && _repairWorkers.size())
	{
		_repairWorkers.clear();
	}

	// If we have no workers, don't bother running any of this
	if (!_workers.size())
		return;

	// Are we expanding to an island base?
	if (_infoManager.shouldIslandExpand())
	{
		if (!_infoManager.getIslandBuilder())
		{
			for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
			{
				if (!it->first || !it->first->exists())
					continue;

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

			if (builder || builder->exists())
			{
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
	}
	else if (_infoManager.getIslandBuilder() && _infoManager.getIslandBuilder()->exists())
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
	}

	// Clear out blocking minerals that are close to our command centers.
	if (mineralsNeedClearing.size() && Broodwar->getFrameCount() > 10000)
	{
		if (_mineralClearer && _mineralClearer->exists())
		{
			if (closeEnough(_mineralClearer->getPosition(), mineralsNeedClearing.front()->getPosition()) && !_mineralClearer->isGatheringMinerals())
				_mineralClearer->rightClick(mineralsNeedClearing.front());
			else if (!closeEnough(_mineralClearer->getPosition(), mineralsNeedClearing.front()->getPosition()))
				_mineralClearer->move(mineralsNeedClearing.front()->getPosition());
		}
		else
		{
			assignMineralClearer(_workers, mineralsNeedClearing, _ownedBases);
		}
	}
	else if (_mineralClearer)
	{
		if (_mineralClearer && _mineralClearer->exists())
			_workers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(_mineralClearer, _infoManager.getOwnedBases().begin()->second));
		_mineralClearer = NULL;
	}
	
	// If we have no owned bases don't bother checking for assignments
	if (_infoManager.getOwnedBases().size())
	{
		// Check each worker's assignment (i.e. gathering minerals/gas)
		for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
		{
			if (!it->first || !it->first->exists())
				continue;

			if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
			{
				Broodwar << it->first->getType() << " found in worker list." << std::endl;
				continue;
			}

			if (it->first->isIdle())
			{
				bool blockerDetected = false;
				for (auto base : _infoManager.getOtherBases())
				{
					if (it->first->getDistance(base.first) < 64)
					{
						blockerDetected = true;
						break;
					}
				}

				if (blockerDetected)
				{
					if (it->first->getPosition().x - 64 < 0)
						it->first->move(BWAPI::Position(it->first->getPosition().x - 64, it->first->getPosition().y));
					else
						it->first->move(BWAPI::Position(it->first->getPosition().x + 128, it->first->getPosition().y));

					continue;
				}
			}

			// Redundant check to make sure all workers are assigned to non-destroyed bases
			bool found = false;
			BWEM::Base * workerBase = _infoManager.getOwnedBases().begin()->second;

			for (auto base : _infoManager.getOwnedBases())
			{
				if (it->second == base.second)
				{
					found = true;
					workerBase = base.second;
					break;
				}
			}

			// If we don't find our base in our list, assign the worker to the first base in our list
			// else we check our assignment
			if (!found)
			{
				// ToDo: We still don't have a good workaround for reassigning workers with no owned bases. Will crash the bot.
				// Though, at that point, the chances of winning would be slim.
				if (_infoManager.getOwnedBases().size())
					it->second = _infoManager.getOwnedBases().begin()->second;
			}
			else if (it->first->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit && it->first->getDistance(BWAPI::Position(_infoManager.getMainPosition())) > 800)
			{
				workerBase->checkAssignment(it->first, _infoManager.getOwnedBases(), it->second);
			}
			else if (it->first->isIdle() || 
				(workerBase->baseHasRefinery() && !workerBase->getBaseRefinery()->isConstructing() && workerBase->getNumGasWorkers() < 3) ||
				(it->first->isGatheringMinerals() && !workerBase->miningAssignmentExists(it->first)))
			{
				workerBase->checkAssignment(it->first, _infoManager.getOwnedBases(), it->second);
			}
		}
	}
	// If we don't have island bases don't bother checking assignments
	if (_infoManager.getIslandBases().size())
	{
		for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _infoManager.getIslandWorkers().begin(); it != _infoManager.getIslandWorkers().end(); it++)
		{
			if ((it->first->isIdle() || it->first->isAttacking()) || (it->second->baseHasRefinery() && it->second->getNumGasWorkers() < 3) ||
				(it->first->isGatheringMinerals() && !it->second->miningAssignmentExists(it->first)))
			{
				it->second->checkAssignment(it->first, _infoManager.getOwnedIslandBases(), it->second);
			}
		}
	}

	// If a building has lost its worker, assign a new one
	for (auto & building : _infoManager.getConstructing())
	{
		/*if (building->isUnderAttack() && building->getHitPoints() < 50)
		{
			building->cancelConstruction();
			continue;
		}*/

		if (building->getBuildUnit() == NULL || !building->getBuildUnit()->exists())
		{
			if (_workers.size())
			{
				int shortestDistance = 999999;
				std::map<BWAPI::Unit, BWEM::Base *>::iterator & builder = _workers.begin();

				for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
				{
					if (!it->first || !it->first->exists())
						continue;

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

				if (builder->first && builder->first->exists())
				{
					builder->first->rightClick(building);

					for (auto & base : _infoManager.getOwnedBases())
					{
						if (base.second == builder->second)
						{
							base.second->removeAssignment(builder->first);
							break;
						}
					}
				}
				else
				{
					// If we didn't find a builder, remove the height and distance requirements
					for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
					{
						if (!it->first || !it->first->exists())
							continue;

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

					if (builder->first && builder->first->exists())
					{
						builder->first->rightClick(building);
						for (auto & base : _infoManager.getOwnedBases())
						{
							if (base.second == builder->second)
							{
								base.second->removeAssignment(builder->first);
								break;
							}
						}
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

void WorkerManager::construct(std::map<BWAPI::Unit, BWEM::Base *>& _workers, BWAPI::UnitType structure, BWAPI::TilePosition targetLocation, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases)
{
	if (_workers.size())
	{
		int shortestDistance = 999999;
		std::map<BWAPI::Unit, BWEM::Base *>::iterator & builder = _workers.begin();

		for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
		{
			if (!it->first || !it->first->exists())
				continue;

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

		if (builder->first && builder->first->exists() && builder != _workers.begin())
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

				for (auto & base : _ownedBases)
				{
					if (base.second == builder->second)
					{
						base.second->removeAssignment(builder->first);
						break;
					}
				}
			}
			else
			{
				builder->first->move(BWAPI::Position(targetLocation));
				for (auto & base : _ownedBases)
				{
					if (base.second == builder->second)
					{
						base.second->removeAssignment(builder->first);
						break;
					}
				}
			}
		}
		else
		{
			// If we didn't find a builder, remove the height and distance requirements
			for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
			{
				if (!it->first || !it->first->exists())
					continue;

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

			if (builder->first && builder->first->exists())
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

					for (auto & base : _ownedBases)
					{
						if (base.second == builder->second)
						{
							base.second->removeAssignment(builder->first);
							break;
						}
					}
				}
				else
				{
					builder->first->move(BWAPI::Position(targetLocation));

					for (auto & base : _ownedBases)
					{
						if (base.second == builder->second)
						{
							base.second->removeAssignment(builder->first);
							break;
						}
					}
				}
			}
		}
	}
}

void WorkerManager::supplyConstruction(std::map<BWAPI::Unit, BWEM::Base *>& _workers, BWAPI::TilePosition targetBuildLocation, int reservedMinerals, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases)
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
			if (!it->first || !it->first->exists())
				continue;

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

		if (builder->first && builder->first->exists())
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

				for (auto & base : _ownedBases)
				{
					if (base.second == builder->second)
					{
						base.second->removeAssignment(builder->first);
						break;
					}
				}
			}
			else
			{
				builder->first->move(BWAPI::Position(targetBuildLocation));
				for (auto & base : _ownedBases)
				{
					if (base.second == builder->second)
					{
						base.second->removeAssignment(builder->first);
						break;
					}
				}
			}
		}
		else
		{
			// If we didn't find a builder, remove the height and distance requirements
			for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
			{
				if (!it->first || !it->first->exists())
					continue;

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

			if (builder->first && builder->first->exists())
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

					for (auto & base : _ownedBases)
					{
						if (base.second == builder->second)
						{
							base.second->removeAssignment(builder->first);
							break;
						}
					}
				}
				else
				{
					builder->first->move(BWAPI::Position(targetBuildLocation));

					for (auto & base : _ownedBases)
					{
						if (base.second == builder->second)
						{
							base.second->removeAssignment(builder->first);
							break;
						}
					}
				}
			}
		}
	}
}

void insanitybot::WorkerManager::assignBullyHunters(std::map<BWAPI::Unit, BWEM::Base*>& _workers, std::list<BWAPI::Unit>& _bullyHunters, int numberOfEnemies, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases)
{
	if (_workers.size())
	{
		std::map<BWAPI::Unit, BWEM::Base *>::iterator & hunter = _workers.begin();

		if (hunter->first && hunter->first->exists())
		{
			if (hunter->first->getType() != BWAPI::UnitTypes::Terran_SCV)
			{
				Broodwar << hunter->first->getType() << " found in worker list." << std::endl;
				return;
			}

			_bullyHunters.push_back(hunter->first);

			for (auto & base : _ownedBases)
			{
				if (base.second == hunter->second)
				{
					/*BWAPI::Broodwar << "base min before: " << base.second->getNumMineralWorkers() << std::endl;
					BWAPI::Broodwar << "base gas before: " << base.second->getNumGasWorkers() << std::endl;
					base.second->removeAssignment(hunter->first);
					BWAPI::Broodwar << "base min after: " << base.second->getNumMineralWorkers() << std::endl;
					BWAPI::Broodwar << "base gas after: " << base.second->getNumGasWorkers() << std::endl;*/

					for (auto & assignment : base.second->getMineralAssignments())
					{
						for (auto & mineralWorker : assignment.second)
						{
							if (mineralWorker == hunter->first)
							{
								assignment.second.erase(mineralWorker);
								goto outOfFor;
							}
						}
					}

					for (auto & assignment : base.second->getRefineryAssignments())
					{
						for (auto & gasWorker : assignment.second)
						{
							if (gasWorker == hunter->first)
							{
								assignment.second.erase(gasWorker);
								goto outOfFor;
							}
						}
					}

					outOfFor:
					break;
				}
			}

			_workers.erase(hunter);
		}
	}

	if (_bullyHunters.size() < numberOfEnemies * 2 && _workers.size())
	{
		assignBullyHunters(_workers, _bullyHunters, numberOfEnemies, _ownedBases);
	}
}

void insanitybot::WorkerManager::assignRepairWorkers(std::map<BWAPI::Unit, BWEM::Base *>& _workers, std::list<BWAPI::Unit>& _repairWorkers, BWAPI::Unit building, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases)
{
	if (_workers.size() > 3)
	{
		int shortestDistance = 999999;
		std::map<BWAPI::Unit, BWEM::Base *>::iterator & repairer = _workers.begin();

		for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
		{
			if (!it->first || !it->first->exists())
				continue;

			if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
			{
				Broodwar << it->first->getType() << " found in worker list." << std::endl;
				continue;
			}

			if (it->first->getType().isWorker() && !it->first->isConstructing() &&
				!it->first->isCarryingGas() && !it->first->isGatheringGas() &&
				it->first->getDistance(building->getPosition()) < shortestDistance)
			{
				shortestDistance = it->first->getDistance(building->getPosition());
				repairer = it;
			}
		}

		if (repairer->first && repairer->first->exists())
		{
			_repairWorkers.push_back(repairer->first);

			for (auto & base : _ownedBases)
			{
				if (base.second == repairer->second)
				{
					base.second->removeAssignment(repairer->first);
					break;
				}
			}

			_workers.erase(repairer);
		}
	}
	else
	{
		return;
	}

	if ((building->getType() == BWAPI::UnitTypes::Terran_Bunker || building->getType() == BWAPI::UnitTypes::Terran_Missile_Turret) && _repairWorkers.size() < 3)
	{
		assignRepairWorkers(_workers, _repairWorkers, building, _ownedBases);
	}
}

void insanitybot::WorkerManager::assignMineralClearer(std::map<BWAPI::Unit, BWEM::Base*>& _workers, std::vector<BWAPI::Unit> needClearing, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases)
{
	if (_workers.size() && needClearing.size())
	{
		int shortestDistance = 999999;
		std::map<BWAPI::Unit, BWEM::Base *>::iterator & clearer = _workers.begin();

		for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
		{
			if (!it->first || !it->first->exists())
				continue;

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

		if (clearer->first->exists())
		{
			_mineralClearer = clearer->first;

			for (auto & base : _ownedBases)
			{
				if (base.second == clearer->second)
				{
					base.second->removeAssignment(clearer->first);
					break;
				}
			}

			_workers.erase(clearer);
		}
	}
}

void insanitybot::WorkerManager::handleIslandConstruction(std::map<BWAPI::Unit, BWEM::Base*>& _islandWorkers, std::map<BWAPI::Position, BWEM::Base *> & _ownedIslandBases, std::list<BWAPI::Unit> _engibays, BWAPI::TilePosition targetLocation)
{
	for (auto base : _ownedIslandBases)
	{
		if (base.second->baseHasGeyser() && !base.second->baseHasRefinery())
		{
			for (auto worker : _islandWorkers)
			{
				if (worker.second == base.second && worker.first->isGatheringMinerals())
				{
					if (BWAPI::Broodwar->self()->minerals() >= 100)
					{
						worker.first->build(BWAPI::UnitTypes::Terran_Refinery, base.second->Geysers().front()->TopLeft());
					}
					break;
				}
			}
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

WorkerManager & WorkerManager::Instance()
{
	static WorkerManager instance;
	return instance;
}