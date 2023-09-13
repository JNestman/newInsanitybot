#include "CreationManager.h"

using namespace insanitybot;

insanitybot::CreationManager::CreationManager()
	: _self(BWAPI::Broodwar->self())
{
}

void CreationManager::initialize()
{
	_buildingPlacer.initialize();

	_constructionQueue.clear();
}

void CreationManager::update(InformationManager & _infoManager)
{
	int mineralsLeft = _self->minerals();
	int gasLeft = _self->gas();
	bool wantSilo = _infoManager.getStrategy() == "Nuke" && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Nuclear_Silo) < 1;
	int comsatsBuilt = 0;
	int supplyLeft = _self->supplyTotal() - _self->supplyUsed();

	/******************************************************
		* SCV good to go sir, SCV good to go sir, SCV good to go sir,
		* SCV good to go sir, SCV good to go sir, SCV good to go sir,
		* SCV good to go sir, SCV good to go sir, SCV good to go sir...
		*******************************************************/
	for (auto & center : _infoManager.getCommandCenters())
	{
		if (!center || !center->exists())
			continue;

		if (center->isUnderAttack())
			continue;

		if (_infoManager.getWorkers().size() >= 7 && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) < 1)
			continue;

		if (wantSilo && center->isIdle() && _infoManager.getScience().size() && _infoManager.covertOpsDone() &&
			center->canBuildAddon(BWAPI::UnitTypes::Terran_Nuclear_Silo) && 
			mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Comsat_Station.mineralPrice() &&
			gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Comsat_Station.gasPrice() &&
			center->getHitPoints() == BWAPI::UnitTypes::Terran_Command_Center.maxHitPoints())
		{
			center->buildAddon(BWAPI::UnitTypes::Terran_Nuclear_Silo);
			mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Nuclear_Silo.mineralPrice();
			gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Nuclear_Silo.gasPrice();
		}
		else if (center->isIdle() && center->canBuildAddon(BWAPI::UnitTypes::Terran_Comsat_Station) && _infoManager.getAcademy().size() &&
			mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Comsat_Station.mineralPrice() &&
			gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Comsat_Station.gasPrice() &&
			center->getHitPoints() == BWAPI::UnitTypes::Terran_Command_Center.maxHitPoints() &&
			((_infoManager.getComsats().size() < 1 && comsatsBuilt < 1) || !wantSilo))
		{
			center->buildAddon(BWAPI::UnitTypes::Terran_Comsat_Station);
			mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Comsat_Station.mineralPrice();
			gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Comsat_Station.gasPrice();
			comsatsBuilt++;
		}
		else if (center->isIdle() && mineralsLeft - _infoManager.getReservedMinerals() >= center->getType().getRace().getWorker().mineralPrice() 
			&& _infoManager.getNumWorkersOwned() < _infoManager.numWorkersWanted() &&
			center->getType().getRace().getWorker().supplyRequired() <= supplyLeft)
		{
			center->train(center->getType().getRace().getWorker());
			mineralsLeft = mineralsLeft - center->getType().getRace().getWorker().mineralPrice();
			supplyLeft -= center->getType().getRace().getWorker().supplyRequired();
		}
	}

	// Island turret and refinery construction
	if(_infoManager.getIslandBases().size())
	{
		for (auto & center : _infoManager.getIslandCenters())
		{
			if (center->isIdle() && center->canBuildAddon(BWAPI::UnitTypes::Terran_Comsat_Station) && _infoManager.getAcademy().size() &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Comsat_Station.mineralPrice() &&
				gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Comsat_Station.gasPrice() &&
				center->getHitPoints() == BWAPI::UnitTypes::Terran_Command_Center.maxHitPoints())
			{
				center->buildAddon(BWAPI::UnitTypes::Terran_Comsat_Station);
				mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Comsat_Station.mineralPrice();
				gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Comsat_Station.gasPrice();
			}
			else if (center->isIdle() && mineralsLeft - _infoManager.getReservedMinerals() >= center->getType().getRace().getWorker().mineralPrice() && 
				_infoManager.getIslandWorkers().size() < _infoManager.getOwnedIslandBases().size() * 15 &&
				center->getType().getRace().getWorker().supplyRequired() <= supplyLeft)
			{
				center->train(center->getType().getRace().getWorker());
				mineralsLeft = mineralsLeft - center->getType().getRace().getWorker().mineralPrice();
				supplyLeft -= center->getType().getRace().getWorker().supplyRequired();
			}
		}

		for (auto base : _infoManager.getOwnedIslandBases())
		{
			if (base.second->baseHasGeyser() && !base.second->baseHasRefinery() && 
				(base.second->getBaseCommandCenter() && base.second->getBaseCommandCenter()->exists()))
			{
				if (!base.second->getBaseCommandCenter()->isBeingConstructed())
				{
					BWAPI::TilePosition islandBuildingLocation = _buildingPlacer.getPositionNear(BWAPI::UnitTypes::Terran_Missile_Turret, base.second->Location(), _infoManager.isMech(_infoManager.getStrategy()));
					WorkerManager::Instance().handleIslandConstruction(_infoManager.getIslandWorkers(), _infoManager.getOwnedIslandBases(), _infoManager.getEngibays(), islandBuildingLocation);
				}
			}
		}
	}

	/******************************************************
	* This is where we will construct what
	* we put into our queue.
	*******************************************************/
	if (!_infoManager.getQueue().empty() && _constructionQueue.empty())
	{
		std::list<BWAPI::TilePosition> takenPositions;
		takenPositions.clear();
		for (BWAPI::UnitType unit : _infoManager.getQueue())
		{
			// Trying to add supply seperately to the reserved queue without messing up it's separate handling
			if (unit == BWAPI::UnitTypes::Terran_Supply_Depot)
				continue;

			// This may be flawed but it makes sense for now. May run into problems if we have a large pool of reserved minerals/gas but it remains to be seen.
			if (unit.isBuilding() && mineralsLeft >= unit.mineralPrice() && gasLeft >= unit.gasPrice())
			{
				BWAPI::TilePosition targetBuildingLocation;
				if (unit == BWAPI::UnitTypes::Terran_Missile_Turret)
					targetBuildingLocation = _buildingPlacer.getTurretLocation(_infoManager);
				else
					targetBuildingLocation = _buildingPlacer.getDesiredLocation(unit, _infoManager, takenPositions);

				takenPositions.push_back(targetBuildingLocation);
				WorkerManager::Instance().construct(_infoManager.getWorkers(), unit, targetBuildingLocation, _infoManager.getOwnedBases());
				_constructionQueue.insert(std::pair<BWAPI::UnitType, int>(unit, Broodwar->getFrameCount()));
				mineralsLeft = mineralsLeft - unit.mineralPrice();
				gasLeft = gasLeft - unit.gasPrice();
			}
			else if (unit == BWAPI::UnitTypes::Terran_Nuclear_Missile && mineralsLeft >= unit.mineralPrice() && gasLeft >= unit.gasPrice())
			{
				bool siloFound = false;

				for (auto & addon : _infoManager.getAddons())
				{
					if (addon->getType() == BWAPI::UnitTypes::Terran_Nuclear_Silo && addon->isIdle() &&
						BWAPI::UnitTypes::Terran_Nuclear_Missile.supplyRequired() <= supplyLeft)
					{
						addon->train(BWAPI::UnitTypes::Terran_Nuclear_Missile);
						mineralsLeft = mineralsLeft - unit.mineralPrice();
						gasLeft = gasLeft - unit.gasPrice();
						supplyLeft -= BWAPI::UnitTypes::Terran_Nuclear_Missile.supplyRequired();

						for (std::list<BWAPI::UnitType>::iterator queued = _infoManager.getQueue().begin(); queued != _infoManager.getQueue().end(); queued++)
						{
							if (*queued == unit)
							{
								_infoManager.getQueue().erase(queued);
								_infoManager.getAndAlterMinerals() = _infoManager.getAndAlterMinerals() - unit.mineralPrice();
								_infoManager.getAndAlterGas() = _infoManager.getAndAlterGas() - unit.gasPrice();
								return;
							}
						}
					}
				}
			}
			
			if (unit == BWAPI::UnitTypes::Terran_Bunker && (!_infoManager.getBarracks().size() || _infoManager.getBunkers().size() == 2))
			{
				std::list<BWAPI::UnitType>& queue = _infoManager.getQueue();
				for (std::list<BWAPI::UnitType>::iterator it = queue.begin(); it != queue.end(); it++)
				{
					if (*it == unit)
					{
						_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() - BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
						queue.erase(it);

						for (std::map<BWAPI::UnitType, int>::iterator constructionQueue = _constructionQueue.begin(); constructionQueue != _constructionQueue.end(); ++constructionQueue)
						{
							if (constructionQueue->first == *it)
							{
								_constructionQueue.erase(constructionQueue);
								break;
							}
						}

						break;
					}
				}
			}

			//These checks are for items that require other buildings in order to be constructed
			//If the required buildings do not exist then we should not try to build them and get stuck.
			if (unit == BWAPI::UnitTypes::Terran_Missile_Turret && !_infoManager.getEngibays().size())
			{
				std::list<BWAPI::UnitType>& queue = _infoManager.getQueue();
				for (std::list<BWAPI::UnitType>::iterator it = queue.begin(); it != queue.end(); it++)
				{
					if (*it == unit)
					{
						_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() - BWAPI::UnitTypes::Terran_Missile_Turret.mineralPrice());
						queue.erase(it);

						for (std::map<BWAPI::UnitType, int>::iterator constructionQueue = _constructionQueue.begin(); constructionQueue != _constructionQueue.end(); ++constructionQueue)
						{
							if (constructionQueue->first == *it)
							{
								_constructionQueue.erase(constructionQueue);
								break;
							}
						}

						break;
					}
				}
			}

			if (unit == BWAPI::UnitTypes::Terran_Nuclear_Missile && !_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Nuclear_Silo))
			{
				std::list<BWAPI::UnitType>& queue = _infoManager.getQueue();
				for (std::list<BWAPI::UnitType>::iterator it = queue.begin(); it != queue.end(); it++)
				{
					if (*it == unit)
					{
						_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() - BWAPI::UnitTypes::Terran_Nuclear_Missile.mineralPrice());
						_infoManager.setReservedGas(_infoManager.getReservedGas() - BWAPI::UnitTypes::Terran_Nuclear_Missile.gasPrice());
						queue.erase(it);

						for (std::map<BWAPI::UnitType, int>::iterator constructionQueue = _constructionQueue.begin(); constructionQueue != _constructionQueue.end(); ++constructionQueue)
						{
							if (constructionQueue->first == *it)
							{
								_constructionQueue.erase(constructionQueue);
								break;
							}
						}

						break;
					}
				}
			}

			if (unit == BWAPI::UnitTypes::Terran_Science_Facility && !_infoManager.getStarports().size())
			{
				std::list<BWAPI::UnitType>& queue = _infoManager.getQueue();
				for (std::list<BWAPI::UnitType>::iterator it = queue.begin(); it != queue.end(); it++)
				{
					if (*it == unit)
					{
						_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() - BWAPI::UnitTypes::Terran_Science_Facility.mineralPrice());
						_infoManager.setReservedGas(_infoManager.getReservedGas() - BWAPI::UnitTypes::Terran_Science_Facility.gasPrice());
						queue.erase(it);

						for (std::map<BWAPI::UnitType, int>::iterator constructionQueue = _constructionQueue.begin(); constructionQueue != _constructionQueue.end(); ++constructionQueue)
						{
							if (constructionQueue->first == *it)
							{
								_constructionQueue.erase(constructionQueue);
								break;
							}
						}

						break;
					}
				}
			}

			if (unit == BWAPI::UnitTypes::Terran_Academy && !_infoManager.getBarracks().size())
			{
				std::list<BWAPI::UnitType>& queue = _infoManager.getQueue();
				for (std::list<BWAPI::UnitType>::iterator it = queue.begin(); it != queue.end(); it++)
				{
					if (*it == unit)
					{
						_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() - BWAPI::UnitTypes::Terran_Academy.mineralPrice());
						queue.erase(it);

						for (std::map<BWAPI::UnitType, int>::iterator constructionQueue = _constructionQueue.begin(); constructionQueue != _constructionQueue.end(); ++constructionQueue)
						{
							if (constructionQueue->first == *it)
							{
								_constructionQueue.erase(constructionQueue);
								break;
							}
						}

						break;
					}
				}
			}
		}
	}

	/****************************************************
	* Check our idle production and put them to work
	*****************************************************/
	int numMarines = _infoManager.getMarines().size();
	int numGhosts = _infoManager.getGhosts().size();

	// We want a ratio of one firebat every ten marines
	int numFirebatsWanted = 0;
	if (numMarines >= 10)
		numFirebatsWanted = numMarines / 10;

	// We want a ration of one medic per three or four marines pending strat
	int numMedicsWanted = 0;
	if (numMarines >= 10)
	{
		if (_infoManager.isAllIn(_infoManager.getStrategy()))
			numMedicsWanted = numMarines / 3;
		else
			numMedicsWanted = numMarines / 4;
	}

	for (auto & rax : _infoManager.getBarracks())
	{
		if (_infoManager.isBio(_infoManager.getStrategy()) || (_infoManager.isAllIn(_infoManager.getStrategy()) && _infoManager.getStrategy() != "MechAllIn"))
		{
			if (rax->exists() && rax->isIdle())
			{
				if (_infoManager.getStrategy() == "Nuke" && 
					_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Academy) &&
					_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility) && 
					_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Covert_Ops) && 
					numGhosts < 4 && _infoManager.getMarines().size() > 24 &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Ghost.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Ghost.gasPrice() &&
					BWAPI::UnitTypes::Terran_Ghost.supplyRequired() <= supplyLeft)
				{
					rax->train(BWAPI::UnitTypes::Terran_Ghost);
					mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Ghost.mineralPrice();
					gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Ghost.gasPrice();
					numGhosts += 1;
					supplyLeft -= BWAPI::UnitTypes::Terran_Ghost.supplyRequired();
				}
				else if (numMedicsWanted > _infoManager.getMedics().size() && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Academy) > 0 &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Medic.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Medic.gasPrice() &&
					BWAPI::UnitTypes::Terran_Medic.supplyRequired() <= supplyLeft)
				{
					rax->train(BWAPI::UnitTypes::Terran_Medic);
					mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Medic.mineralPrice();
					gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Medic.gasPrice();
					numMedicsWanted -= 1;
					supplyLeft -= BWAPI::UnitTypes::Terran_Medic.supplyRequired();
				}
				else if (!_infoManager.isAllIn(_infoManager.getStrategy()) &&
					numFirebatsWanted > _infoManager.getFirebats().size() && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Academy) > 0 &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Firebat.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Firebat.gasPrice() &&
					BWAPI::UnitTypes::Terran_Firebat.supplyRequired() <= supplyLeft)
				{
					rax->train(BWAPI::UnitTypes::Terran_Firebat);
					mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Firebat.mineralPrice();
					gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Firebat.gasPrice();
					numFirebatsWanted -= 1;
					supplyLeft -= BWAPI::UnitTypes::Terran_Firebat.supplyRequired();
				}
				else if (mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Marine.mineralPrice() &&
					BWAPI::UnitTypes::Terran_Marine.supplyRequired() <= supplyLeft)
				{
					rax->train(BWAPI::UnitTypes::Terran_Marine);
					mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Marine.mineralPrice();
					supplyLeft -= BWAPI::UnitTypes::Terran_Marine.supplyRequired();
				}
			}
		}
		else
		{
			if (rax->exists())
			{
				if (rax->isIdle() && _infoManager.getMarines().size() + BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Terran_Marine) < 4)
				{
					if (mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Marine.mineralPrice() &&
						BWAPI::UnitTypes::Terran_Marine.supplyRequired() <= supplyLeft)
					{
						rax->train(BWAPI::UnitTypes::Terran_Marine);
						mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Marine.mineralPrice();
						supplyLeft -= BWAPI::UnitTypes::Terran_Marine.supplyRequired();
					}
				}
				else if (!rax->isLifted() && rax->isIdle())
				{
					rax->lift();
				}
			}
		}
	}

	// Roughly, we want the number of machines shops to equal our refineries
	int numMachineShops = _infoManager.getMachineShops().size();
	int numShopsWanted = 0;
	if (_infoManager.isAirStrat(_infoManager.getStrategy()) || _infoManager.getStrategy() == "MechAllIn")
	{
		numShopsWanted = 1;
	}
	else
	{
		for (auto base : _infoManager.getOwnedBases())
		{
			if (base.second->baseHasRefinery())
			{
				numShopsWanted++;
			}
		}
	}

	for (auto & factory : _infoManager.getFactories())
	{
		if (!factory->exists() || factory->getType() != BWAPI::UnitTypes::Terran_Factory)
		{
			BWAPI::Broodwar << "Invalid type found in factory list" << std::endl;
			continue;
		}

		if (factory->isCompleted() && _infoManager.isBio(_infoManager.getStrategy()) && !factory->isLifted())
		{
			factory->lift();
		}
		else if (factory->isCompleted() && _infoManager.isAllIn(_infoManager.getStrategy()) && factory->isIdle())
		{
			if (numMachineShops < numShopsWanted && factory->canBuildAddon() &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Machine_Shop.mineralPrice() &&
				gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Machine_Shop.gasPrice())
			{
				factory->buildAddon(BWAPI::UnitTypes::Terran_Machine_Shop);
				mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Machine_Shop.mineralPrice();
				gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Machine_Shop.gasPrice();
				numShopsWanted -= 1;
			}
			else if (!factory->canBuildAddon() && factory->isIdle() &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.mineralPrice() &&
				gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.gasPrice() &&
				_infoManager.getStrategy() != "MechAllIn" &&
				BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.supplyRequired() <= supplyLeft)
			{
				factory->train(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);
				mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.mineralPrice();
				gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.gasPrice();
				supplyLeft -= BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.supplyRequired();
			}
			else if (factory->isIdle() && (factory->canBuildAddon() || _infoManager.getStrategy() == "MechAllIn") &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Vulture.mineralPrice() &&
				BWAPI::UnitTypes::Terran_Vulture.supplyRequired() <= supplyLeft)
			{
				factory->train(BWAPI::UnitTypes::Terran_Vulture);
				mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Vulture.mineralPrice();
				supplyLeft -= BWAPI::UnitTypes::Terran_Vulture.supplyRequired();
			}
		}
		else if (factory->isCompleted() && _infoManager.isMech(_infoManager.getStrategy()) && factory->isIdle())
		{
			if (numMachineShops < numShopsWanted && factory->canBuildAddon() &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Machine_Shop.mineralPrice() &&
				gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Machine_Shop.gasPrice())
			{
				factory->buildAddon(BWAPI::UnitTypes::Terran_Machine_Shop);
				mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Machine_Shop.mineralPrice();
				gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Machine_Shop.gasPrice();
				numShopsWanted -= 1;
			}
			else if (_infoManager.armoryDone() && factory->isIdle() && _infoManager.enemyHasAir() &&
				_infoManager.getVultures().size() + _infoManager.getGoliaths().size() < _infoManager.getTanks().size() * 2 &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Goliath.mineralPrice() &&
				gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Goliath.gasPrice() &&
				BWAPI::UnitTypes::Terran_Goliath.supplyRequired() <= supplyLeft)
			{
				factory->train(BWAPI::UnitTypes::Terran_Goliath);
				mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Goliath.mineralPrice();
				gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Goliath.gasPrice();
				supplyLeft -= BWAPI::UnitTypes::Terran_Goliath.supplyRequired();
			}
			else if (factory->isIdle() && _infoManager.getVultures().size() + _infoManager.getGoliaths().size() < _infoManager.getTanks().size() * 2 &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Vulture.mineralPrice() &&
				BWAPI::UnitTypes::Terran_Vulture.supplyRequired() <= supplyLeft)
			{
				factory->train(BWAPI::UnitTypes::Terran_Vulture);
				mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Vulture.mineralPrice();
				supplyLeft -= BWAPI::UnitTypes::Terran_Vulture.supplyRequired();
			}
			else if (!factory->canBuildAddon() && factory->isIdle() &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.mineralPrice() &&
				gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.gasPrice() &&
				BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.supplyRequired() <= supplyLeft)
			{
				factory->train(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);
				mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.mineralPrice();
				gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.gasPrice();
				supplyLeft -= BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.supplyRequired();
			}
		}
		else if (factory->isLifted())
		{
			factory->move(BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, BWAPI::Position(_infoManager.getMainPosition()).y));
		}
	}
	for (auto & starport : _infoManager.getStarports())
	{
		if (starport->getType() == BWAPI::UnitTypes::Terran_Starport && starport->isIdle())
		{
			if (starport->canBuildAddon() &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Control_Tower.mineralPrice() &&
				gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Control_Tower.gasPrice())
			{
				starport->buildAddon(BWAPI::UnitTypes::Terran_Control_Tower);
				mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Control_Tower.mineralPrice();
				gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Control_Tower.gasPrice();
			}
			else if (starport->getAddon() != NULL)
			{
				if (_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility) && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Physics_Lab) &&
					_infoManager.getStrategy() == "BCMeme" &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Battlecruiser.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Battlecruiser.gasPrice() &&
					BWAPI::UnitTypes::Terran_Battlecruiser.supplyRequired() <= supplyLeft)
				{
					starport->train(BWAPI::UnitTypes::Terran_Battlecruiser);
					mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Battlecruiser.mineralPrice();
					gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Battlecruiser.gasPrice();
					supplyLeft -= BWAPI::UnitTypes::Terran_Battlecruiser.supplyRequired();
				}
				if ((_infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Dropship) < 1 || 
					(_infoManager.getStrategy() == "BioDrops" && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Dropship) < 4)) && 
					(_infoManager.getIslandBases().size() || _infoManager.getOwnedIslandBases().size() || 
					BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Terran_Dropship) <= 4) &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Dropship.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Dropship.gasPrice() &&
					BWAPI::UnitTypes::Terran_Dropship.supplyRequired() <= supplyLeft)
				{
					starport->train(BWAPI::UnitTypes::Terran_Dropship);
					mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Dropship.mineralPrice();
					gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Dropship.gasPrice();
					supplyLeft -= BWAPI::UnitTypes::Terran_Dropship.supplyRequired();
				}
				else if (_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility)  && 
					(_infoManager.getStrategy() == "SKTerran" || (_infoManager.getStrategy() == "Nuke" && _infoManager.getEnemyRace() == BWAPI::Races::Zerg)) &&
					_infoManager.getVessels().size() < 14 &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Science_Vessel.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Science_Vessel.gasPrice() &&
					BWAPI::UnitTypes::Terran_Science_Vessel.supplyRequired() <= supplyLeft)
				{
					starport->train(BWAPI::UnitTypes::Terran_Science_Vessel);
					mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Science_Vessel.mineralPrice();
					gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Science_Vessel.gasPrice();
					supplyLeft -= BWAPI::UnitTypes::Terran_Science_Vessel.supplyRequired();
				}
				else if (_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility) && 
					_infoManager.getVessels().size() < 2  && (_infoManager.getStrategy() != "BCMeme" || _infoManager.getBCs().size() > 4) &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Science_Vessel.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Science_Vessel.gasPrice() &&
					BWAPI::UnitTypes::Terran_Science_Vessel.supplyRequired() <= supplyLeft)
				{
					starport->train(BWAPI::UnitTypes::Terran_Science_Vessel);
					mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Science_Vessel.mineralPrice();
					gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Science_Vessel.gasPrice();
					supplyLeft -= BWAPI::UnitTypes::Terran_Science_Vessel.supplyRequired();
				}
			}
		}
	}

	/***************************************************
	* Check idle upgrade buildings
	* and put them to work
	****************************************************/
	for (auto & academy : _infoManager.getAcademy())
	{
		if (academy->exists() && academy->isIdle() && (_infoManager.isBio(_infoManager.getStrategy()) || 
			(_infoManager.isAllIn(_infoManager.getStrategy()) && _self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode))))
		{
			if (!_self->getUpgradeLevel(BWAPI::UpgradeTypes::U_238_Shells) &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UpgradeTypes::U_238_Shells.mineralPrice() &&
				gasLeft - _infoManager.getReservedGas() >= BWAPI::UpgradeTypes::U_238_Shells.gasPrice())
			{
				academy->upgrade(BWAPI::UpgradeTypes::U_238_Shells);
				mineralsLeft = mineralsLeft - BWAPI::UpgradeTypes::U_238_Shells.mineralPrice();
				gasLeft = gasLeft - BWAPI::UpgradeTypes::U_238_Shells.gasPrice();
			}
			else if (_self->getUpgradeLevel(BWAPI::UpgradeTypes::U_238_Shells) && !_self->hasResearched(BWAPI::TechTypes::Stim_Packs) &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::TechTypes::Stim_Packs.mineralPrice() &&
				gasLeft - _infoManager.getReservedGas() >= BWAPI::TechTypes::Stim_Packs.gasPrice())
			{
				academy->research(BWAPI::TechTypes::Stim_Packs);
				mineralsLeft = mineralsLeft - BWAPI::TechTypes::Stim_Packs.mineralPrice();
				gasLeft = gasLeft - BWAPI::TechTypes::Stim_Packs.gasPrice();
			}
			else if (_self->hasResearched(BWAPI::TechTypes::Stim_Packs) && !_self->hasResearched(BWAPI::TechTypes::Optical_Flare) &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::TechTypes::Optical_Flare.mineralPrice() &&
				gasLeft - _infoManager.getReservedGas() >= BWAPI::TechTypes::Optical_Flare.gasPrice())
			{
				academy->research(BWAPI::TechTypes::Optical_Flare);
				mineralsLeft = mineralsLeft - BWAPI::TechTypes::Optical_Flare.mineralPrice();
				gasLeft = gasLeft - BWAPI::TechTypes::Optical_Flare.gasPrice();
			}
		}
	}

	for (auto & engibay : _infoManager.getEngibays())
	{
		if (engibay->exists() && engibay->isIdle() && (_infoManager.isBio(_infoManager.getStrategy()) || _infoManager.isAllIn(_infoManager.getStrategy())))
		{
			if (engibay->isLifted())
			{
				engibay->move(BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, BWAPI::Position(_infoManager.getMainPosition()).y));
				continue;
			}

			int infantryWeapons = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Infantry_Weapons);
			int infantryArmor = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Infantry_Armor);

			if (infantryWeapons < 3 && !_self->isUpgrading(BWAPI::UpgradeTypes::Terran_Infantry_Weapons) &&
				mineralsLeft - _infoManager.getReservedMinerals() >= UpgradeTypes::Terran_Infantry_Weapons.mineralPrice(infantryWeapons + 1) &&
				gasLeft - _infoManager.getReservedGas() >= UpgradeTypes::Terran_Infantry_Weapons.gasPrice(infantryWeapons + 1) &&
				(infantryWeapons == 0 || (infantryWeapons > 0 && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility))))
			{
				engibay->upgrade(BWAPI::UpgradeTypes::Terran_Infantry_Weapons);
				mineralsLeft = mineralsLeft - BWAPI::UpgradeTypes::Terran_Infantry_Weapons.mineralPrice(infantryWeapons + 1);
				gasLeft = gasLeft - BWAPI::UpgradeTypes::Terran_Infantry_Weapons.gasPrice(infantryWeapons + 1);
				break;
			}
			else if (infantryArmor < 3 && !_self->isUpgrading(BWAPI::UpgradeTypes::Terran_Infantry_Armor) &&
				mineralsLeft - _infoManager.getReservedMinerals() >= UpgradeTypes::Terran_Infantry_Armor.mineralPrice(infantryArmor + 1) &&
				gasLeft - _infoManager.getReservedGas() >= UpgradeTypes::Terran_Infantry_Armor.gasPrice(infantryArmor + 1) &&
				(infantryArmor == 0 || (infantryArmor > 0 && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility))))
			{
				engibay->upgrade(BWAPI::UpgradeTypes::Terran_Infantry_Armor);
				mineralsLeft = mineralsLeft - BWAPI::UpgradeTypes::Terran_Infantry_Armor.mineralPrice(infantryArmor + 1);
				gasLeft = gasLeft - BWAPI::UpgradeTypes::Terran_Infantry_Armor.gasPrice(infantryArmor + 1);
				break;
			}
			else if (infantryWeapons == 3 && infantryArmor == 3 && !engibay->isLifted())
			{
				engibay->lift();
			}
		}
		else if (engibay->exists() && _infoManager.isMech(_infoManager.getStrategy()))
		{
			if (!engibay->isLifted() && engibay->isIdle())
				engibay->lift();
		}
	}

	for (auto & armory : _infoManager.getArmories())
	{
		if (armory->exists() && armory->isIdle() && !_infoManager.isBio(_infoManager.getStrategy()))
		{
			int mechWeapons = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons);
			int mechArmor = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Vehicle_Plating);
			int airWeapons = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Ship_Weapons);
			int airArmor = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Ship_Plating);

			if (_infoManager.isAirStrat(_infoManager.getStrategy()) &&
				airWeapons < 3 && !_self->isUpgrading(BWAPI::UpgradeTypes::Terran_Ship_Weapons) &&
				mineralsLeft - _infoManager.getReservedMinerals() >= UpgradeTypes::Terran_Ship_Weapons.mineralPrice(airWeapons + 1) &&
				gasLeft - _infoManager.getReservedGas() >= UpgradeTypes::Terran_Ship_Weapons.gasPrice(airWeapons + 1) &&
				(airWeapons == 0 || (airWeapons > 0 && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility))))
			{
				armory->upgrade(BWAPI::UpgradeTypes::Terran_Ship_Weapons);
				mineralsLeft = mineralsLeft - BWAPI::UpgradeTypes::Terran_Ship_Weapons.mineralPrice(airWeapons + 1);
				gasLeft = gasLeft - BWAPI::UpgradeTypes::Terran_Ship_Weapons.gasPrice(airWeapons + 1);
				break;
			}
			else if (_infoManager.isAirStrat(_infoManager.getStrategy()) &&
				airArmor < 3 && !_self->isUpgrading(BWAPI::UpgradeTypes::Terran_Ship_Plating) &&
				mineralsLeft - _infoManager.getReservedMinerals() >= UpgradeTypes::Terran_Ship_Plating.mineralPrice(mechArmor + 1) &&
				gasLeft - _infoManager.getReservedGas() >= UpgradeTypes::Terran_Ship_Plating.gasPrice(mechArmor + 1) &&
				(airArmor == 0 || (airArmor > 0 && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility))))
			{
				armory->upgrade(BWAPI::UpgradeTypes::Terran_Ship_Plating);
				mineralsLeft = mineralsLeft - BWAPI::UpgradeTypes::Terran_Ship_Plating.mineralPrice(airArmor + 1);
				gasLeft = gasLeft - BWAPI::UpgradeTypes::Terran_Ship_Plating.gasPrice(airArmor + 1);
				break;
			}
			else if (mechWeapons < 3 && !_self->isUpgrading(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons) &&
				mineralsLeft - _infoManager.getReservedMinerals() >= UpgradeTypes::Terran_Vehicle_Weapons.mineralPrice(mechWeapons + 1) &&
				gasLeft - _infoManager.getReservedGas() >= UpgradeTypes::Terran_Vehicle_Weapons.gasPrice(mechWeapons + 1) &&
				(mechWeapons == 0 || (mechWeapons > 0 && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility))))
			{
				armory->upgrade(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons);
				mineralsLeft = mineralsLeft - BWAPI::UpgradeTypes::Terran_Vehicle_Weapons.mineralPrice(mechWeapons + 1);
				gasLeft = gasLeft - BWAPI::UpgradeTypes::Terran_Vehicle_Weapons.gasPrice(mechWeapons + 1);
				break;
			}
			else if (mechArmor < 3 && !_self->isUpgrading(BWAPI::UpgradeTypes::Terran_Vehicle_Plating) &&
				mineralsLeft - _infoManager.getReservedMinerals() >= UpgradeTypes::Terran_Vehicle_Plating.mineralPrice(mechArmor + 1) &&
				gasLeft - _infoManager.getReservedGas() >= UpgradeTypes::Terran_Vehicle_Plating.gasPrice(mechArmor + 1) &&
				(mechArmor == 0 || (mechArmor > 0 && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility))))
			{
				armory->upgrade(BWAPI::UpgradeTypes::Terran_Vehicle_Plating);
				mineralsLeft = mineralsLeft - BWAPI::UpgradeTypes::Terran_Vehicle_Plating.mineralPrice(mechArmor + 1);
				gasLeft = gasLeft - BWAPI::UpgradeTypes::Terran_Vehicle_Plating.gasPrice(mechArmor + 1);
				break;
			}
		}
	}

	for (auto & scienceFacility : _infoManager.getScience())
	{
		if (scienceFacility->exists() && scienceFacility->isIdle())
		{
			if (_infoManager.getStrategy() == "Nuke" && scienceFacility->canBuildAddon() &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Covert_Ops.mineralPrice() &&
				gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Covert_Ops.gasPrice())
			{
				scienceFacility->buildAddon(BWAPI::UnitTypes::Terran_Covert_Ops);
				mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Covert_Ops.mineralPrice();
				gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Covert_Ops.gasPrice();
				break;
			}
			else if (_infoManager.getStrategy() == "BCMeme" && scienceFacility->canBuildAddon() &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Physics_Lab.mineralPrice() &&
				gasLeft - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Physics_Lab.gasPrice())
			{
				scienceFacility->buildAddon(BWAPI::UnitTypes::Terran_Physics_Lab);
				mineralsLeft = mineralsLeft - BWAPI::UnitTypes::Terran_Physics_Lab.mineralPrice();
				gasLeft = gasLeft - BWAPI::UnitTypes::Terran_Physics_Lab.gasPrice();
				break;
			}
			else if (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Terran &&
				!_self->hasResearched(BWAPI::TechTypes::Irradiate) && !_self->isResearching(BWAPI::TechTypes::Irradiate) &&
				mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::TechTypes::Irradiate.mineralPrice() &&
				gasLeft - _infoManager.getReservedGas() >= BWAPI::TechTypes::Irradiate.gasPrice())
			{
				scienceFacility->research(BWAPI::TechTypes::Irradiate);
				mineralsLeft = mineralsLeft - BWAPI::TechTypes::Irradiate.mineralPrice();
				gasLeft = gasLeft - BWAPI::TechTypes::Irradiate.gasPrice();
				break;
			}
		}
	}

	for (auto & machineShop : _infoManager.getMachineShops())
	{
		if (machineShop->exists() && machineShop->isIdle())
		{
			if (_infoManager.getStrategy() == "MechAllIn")
			{
				if (!_self->hasResearched(BWAPI::TechTypes::Spider_Mines) && !_self->isResearching(BWAPI::TechTypes::Spider_Mines) &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::TechTypes::Spider_Mines.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::TechTypes::Spider_Mines.gasPrice())
				{
					machineShop->research(BWAPI::TechTypes::Spider_Mines);
					mineralsLeft = mineralsLeft - BWAPI::TechTypes::Spider_Mines.mineralPrice();
					gasLeft = gasLeft - BWAPI::TechTypes::Spider_Mines.gasPrice();
					break;
				}
				else if (_self->hasResearched(BWAPI::TechTypes::Spider_Mines) && 
					!_self->getUpgradeLevel(BWAPI::UpgradeTypes::Ion_Thrusters) && !_self->isUpgrading(BWAPI::UpgradeTypes::Ion_Thrusters) &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UpgradeTypes::Ion_Thrusters &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::UpgradeTypes::Ion_Thrusters)
				{
					machineShop->upgrade(BWAPI::UpgradeTypes::Ion_Thrusters);
					mineralsLeft = mineralsLeft - BWAPI::TechTypes::Spider_Mines.mineralPrice();
					gasLeft = gasLeft - BWAPI::TechTypes::Spider_Mines.gasPrice();
					break;
				}
			}
			else
			{
				if (!_self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode) && !_self->isResearching(BWAPI::TechTypes::Tank_Siege_Mode) &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::TechTypes::Tank_Siege_Mode.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::TechTypes::Tank_Siege_Mode.gasPrice())
				{
					machineShop->research(BWAPI::TechTypes::Tank_Siege_Mode);
					mineralsLeft = mineralsLeft - BWAPI::TechTypes::Tank_Siege_Mode.mineralPrice();
					gasLeft = gasLeft - BWAPI::TechTypes::Tank_Siege_Mode.gasPrice();
					break;
				}
				else if (_self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode) && _infoManager.getBarracks().size() < 2 &&
					!_self->hasResearched(BWAPI::TechTypes::Spider_Mines) && !_self->isResearching(BWAPI::TechTypes::Spider_Mines) &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::TechTypes::Spider_Mines.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::TechTypes::Spider_Mines.gasPrice())
				{
					machineShop->research(BWAPI::TechTypes::Spider_Mines);
					mineralsLeft = mineralsLeft - BWAPI::TechTypes::Spider_Mines.mineralPrice();
					gasLeft = gasLeft - BWAPI::TechTypes::Spider_Mines.gasPrice();
					break;
				}
				else if (_infoManager.getGoliaths().size() > 3 &&
					!_self->getUpgradeLevel(BWAPI::UpgradeTypes::Charon_Boosters) && !_self->isUpgrading(BWAPI::UpgradeTypes::Charon_Boosters) &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UpgradeTypes::Charon_Boosters.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::UpgradeTypes::Charon_Boosters.gasPrice())
				{
					machineShop->upgrade(BWAPI::UpgradeTypes::Charon_Boosters);
					mineralsLeft = mineralsLeft - BWAPI::UpgradeTypes::Charon_Boosters.mineralPrice();
					gasLeft = gasLeft - BWAPI::UpgradeTypes::Charon_Boosters.gasPrice();
					break;
				}
			}
		}
	}

	for (auto & addon : _infoManager.getAddons())
	{
		if (addon->exists() && addon->isIdle())
		{
			if (addon->getType() == BWAPI::UnitTypes::Terran_Covert_Ops)
			{
				if (!_self->hasResearched(BWAPI::TechTypes::Personnel_Cloaking) && !_self->isResearching(BWAPI::TechTypes::Personnel_Cloaking) &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::TechTypes::Personnel_Cloaking.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::TechTypes::Personnel_Cloaking.gasPrice() &&
					_infoManager.isBio(_infoManager.getStrategy()))
				{
					addon->research(BWAPI::TechTypes::Personnel_Cloaking);
					mineralsLeft = mineralsLeft - BWAPI::TechTypes::Personnel_Cloaking.mineralPrice();
					gasLeft = gasLeft - BWAPI::TechTypes::Personnel_Cloaking.gasPrice();
				}
				else if (!_self->hasResearched(BWAPI::TechTypes::Lockdown) && !_self->isResearching(BWAPI::TechTypes::Lockdown) &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::TechTypes::Lockdown.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::TechTypes::Lockdown.gasPrice() &&
					BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg)
				{
					addon->research(BWAPI::TechTypes::Lockdown);
					mineralsLeft = mineralsLeft - BWAPI::TechTypes::Lockdown.mineralPrice();
					gasLeft = gasLeft - BWAPI::TechTypes::Lockdown.gasPrice();
				}
				else if (!_self->getUpgradeLevel(BWAPI::UpgradeTypes::Ocular_Implants) && !_self->isUpgrading(BWAPI::UpgradeTypes::Ocular_Implants) &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UpgradeTypes::Ocular_Implants.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::UpgradeTypes::Ocular_Implants.gasPrice())
				{
					addon->upgrade(BWAPI::UpgradeTypes::Ocular_Implants);
					mineralsLeft = mineralsLeft - BWAPI::UpgradeTypes::Ocular_Implants.mineralPrice();
					gasLeft = gasLeft - BWAPI::UpgradeTypes::Ocular_Implants.gasPrice();
				}
				else if (!_self->getUpgradeLevel(BWAPI::UpgradeTypes::Moebius_Reactor) && !_self->isUpgrading(BWAPI::UpgradeTypes::Moebius_Reactor) &&
					mineralsLeft - _infoManager.getReservedMinerals() >= BWAPI::UpgradeTypes::Moebius_Reactor.mineralPrice() &&
					gasLeft - _infoManager.getReservedGas() >= BWAPI::UpgradeTypes::Moebius_Reactor.gasPrice())
				{
					addon->upgrade(BWAPI::UpgradeTypes::Moebius_Reactor);
					mineralsLeft = mineralsLeft - BWAPI::UpgradeTypes::Moebius_Reactor.mineralPrice();
					gasLeft = gasLeft - BWAPI::UpgradeTypes::Moebius_Reactor.gasPrice();
				}
			}
		}
	}

	/****************************************************
	* If we called a worker to build something but it never
	* happened, cleart it out of the queue and it will be
	* built again.
	*****************************************************/
	if (!_constructionQueue.empty())
	{
		for (std::map<BWAPI::UnitType, int>::iterator it = _constructionQueue.begin(); it != _constructionQueue.end(); ++it)
		{
			int waitTime = 250;

			//if (it->first.isResourceDepot()) waitTime = 400;
			if (it->first == BWAPI::UnitTypes::Terran_Missile_Turret) waitTime = 100;

			if (BWAPI::Broodwar->getFrameCount() - it->second > waitTime)
			{
				_constructionQueue.erase(it);
			}
		}
	}

	// Supply depot construction is called here, seperate from normal construction
	/*if (_infoManager.isTwoBasePlay(_infoManager.getStrategy()) && _infoManager.getOwnedBases().size() < 2 &&
		_infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) == 1)
		return;*/

	int producerSize = _infoManager.getBarracks().size() + (_infoManager.getFactories().size() * 2) + _infoManager.getStarports().size() + _infoManager.getCommandCenters().size();

	if (WorkerManager::Instance().checkSupplyConstruction(producerSize, _infoManager.getReservedMinerals()))
	{
		BWAPI::TilePosition targetSupplyLocation = _buildingPlacer.getSupplyLocation(BWAPI::UnitTypes::Terran_Supply_Depot, _infoManager);

		_infoManager.getQueue().push_back(BWAPI::UnitTypes::Terran_Supply_Depot);

		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Supply_Depot.mineralPrice());

		WorkerManager::Instance().supplyConstruction(_infoManager.getWorkers(), targetSupplyLocation, _infoManager.getReservedMinerals(), _infoManager.getOwnedBases());
	}
	else if (WorkerManager::Instance().getLastCheckSupply() + 300 < Broodwar->getFrameCount())
	{
		for (std::list<BWAPI::UnitType>::iterator queued = _infoManager.getQueue().begin(); queued != _infoManager.getQueue().end(); queued++)
		{
			if (*queued == BWAPI::UnitTypes::Terran_Supply_Depot)
			{
				_infoManager.getQueue().erase(queued);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() - BWAPI::UnitTypes::Terran_Supply_Depot.mineralPrice());
				break;
			}
		}
	}
}

void CreationManager::checkQueue(BWAPI::Unit unit, std::list<BWAPI::UnitType>& queue, int &reservedMinerals, int &reservedGas)
{
	for (std::map<BWAPI::UnitType, int>::iterator it = _constructionQueue.begin(); it != _constructionQueue.end();)
	{
		if (unit->getType() == it->first)
		{
			for (std::list<BWAPI::UnitType>::iterator queuedUnit = queue.begin(); queuedUnit != queue.end(); ++queuedUnit)
			{
				if (unit->getType() == *queuedUnit)
				{
					queue.erase(queuedUnit);
					break;
				}
			}
			it = _constructionQueue.erase(it);
			reservedMinerals = reservedMinerals - unit->getType().mineralPrice();
			reservedGas = reservedGas - unit->getType().gasPrice();
		}
		else
		{
			it++;
		}
	}
}

CreationManager & CreationManager::Instance()
{
	static CreationManager instance;
	return instance;
}