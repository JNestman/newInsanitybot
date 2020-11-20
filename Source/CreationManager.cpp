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
	/******************************************************
		* SCV good to go sir, SCV good to go sir, SCV good to go sir,
		* SCV good to go sir, SCV good to go sir, SCV good to go sir,
		* SCV good to go sir, SCV good to go sir, SCV good to go sir...
		*******************************************************/
	for (auto & center : _infoManager.getCommandCenters())
	{
		if (center->isUnderAttack())
			continue;

		if (center->isIdle() && center->canBuildAddon(BWAPI::UnitTypes::Terran_Comsat_Station) && _infoManager.getAcademy().size() &&
			_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Comsat_Station.mineralPrice() &&
			_self->gas() - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Comsat_Station.gasPrice())
		{
			center->buildAddon(BWAPI::UnitTypes::Terran_Comsat_Station);
		}
		else if (center->isIdle() && _self->minerals() - _infoManager.getReservedMinerals() >= 50 && _infoManager.getNumWorkersOwned() < _infoManager.numWorkersWanted())
		{
			center->train(center->getType().getRace().getWorker());
		}
	}
	for (auto & center : _infoManager.getIslandCenters())
	{
		if (center->isIdle() && _self->minerals() - _infoManager.getReservedMinerals() >= 50 && _infoManager.getNumWorkersOwned() < _infoManager.numWorkersWanted())
		{
			for (auto base : _infoManager.getOwnedIslandBases())
			{
				if (abs(base.first.x - center->getPosition().x) <= 64 && abs(base.first.y - center->getPosition().y) <= 64 &&
					base.second->getNumMineralWorkers() + base.second->getNumGasWorkers() < base.second->numWorkersWantedHere() - 8)
					center->train(center->getType().getRace().getWorker());
			}
		}
	}
	/******************************************************
	* This is where we will construct what
	* we put into our queue.
	*******************************************************/
	if (!_infoManager.getQueue().empty() && _constructionQueue.empty())
	{
		int spentMinerals = 0;
		int spentGas = 0;
		for (BWAPI::UnitType unit : _infoManager.getQueue())
		{
			// This may be flawed but it makes sense for now. May run into problems if we have a large pool of reserved minerals/gas but it remains to be seen.
			if (unit.isBuilding() && _self->minerals() - spentMinerals >= unit.mineralPrice() && _self->gas() - spentGas >= unit.gasPrice())
			{
				BWAPI::TilePosition targetBuildingLocation;
				if (unit == BWAPI::UnitTypes::Terran_Missile_Turret)
					targetBuildingLocation = _buildingPlacer.getTurretLocation(_infoManager);
				else
					targetBuildingLocation = _buildingPlacer.getDesiredLocation(unit, _infoManager);

				WorkerManager::Instance().construct(_infoManager.getWorkers(), unit, targetBuildingLocation);
				_constructionQueue.insert(std::pair<BWAPI::UnitType, int>(unit, Broodwar->getFrameCount()));
				spentMinerals += unit.mineralPrice();
				spentGas += unit.gasPrice();
			}
			
			if (unit == BWAPI::UnitTypes::Terran_Bunker && _infoManager.getBunkers().size())
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

			if (unit == BWAPI::UnitTypes::Terran_Missile_Turret && (!_infoManager.getEngibays().size() || BWAPI::Broodwar->getFrameCount() % 100 == 0))
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

			if (unit == BWAPI::UnitTypes::Terran_Science_Facility && !_infoManager.getStarports().size())
			{
				std::list<BWAPI::UnitType>& queue = _infoManager.getQueue();
				for (std::list<BWAPI::UnitType>::iterator it = queue.begin(); it != queue.end(); it++)
				{
					if (*it == unit)
					{
						_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() - BWAPI::UnitTypes::Terran_Missile_Turret.mineralPrice());
						_infoManager.setReservedGas(_infoManager.getReservedGas() - BWAPI::UnitTypes::Terran_Missile_Turret.gasPrice());
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
	for (auto & rax : _infoManager.getBarracks())
	{
		if (_infoManager.getStrategy() == "SKTerran")
		{
			if (rax->exists() && rax->isIdle())
			{
				int numMedicsWanted = 0;
				if (_infoManager.getMarines().size() >= 10)
					numMedicsWanted = _infoManager.getMarines().size() / 4;

				if (numMedicsWanted > _infoManager.getMedics().size() && _infoManager.getAcademy().size() &&
					_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Medic.mineralPrice() &&
					_self->gas() - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Medic.gasPrice())
				{
					rax->train(BWAPI::UnitTypes::Terran_Medic);
				}
				else if (_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Marine.mineralPrice())
				{
					rax->train(BWAPI::UnitTypes::Terran_Marine);
				}
			}
		}
		else
		{
			if (rax->exists())
			{
				if (rax->isIdle() && _infoManager.getMarines().size() + BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Terran_Marine) < 4)
				{
					if (_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Marine.mineralPrice())
					{
						rax->train(BWAPI::UnitTypes::Terran_Marine);
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
	for (auto base : _infoManager.getOwnedBases())
	{
		if (base.second->baseHasRefinery())
		{
			numShopsWanted++;
		}
	}
	for (auto & factory : _infoManager.getFactories())
	{
		if (!factory->exists() || factory->getType() != BWAPI::UnitTypes::Terran_Factory)
		{
			BWAPI::Broodwar << "Invalid type found in factory list" << std::endl;
			continue;
		}

		if (factory->isCompleted() && _infoManager.getStrategy() == "SKTerran" && !factory->isLifted())
		{
			factory->lift();
		}
		else if (factory->isCompleted() && _infoManager.getStrategy() == "Mech" && factory->isIdle())
		{
			if (numMachineShops < numShopsWanted && factory->canBuildAddon() &&
				_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Machine_Shop.mineralPrice() &&
				_self->gas() - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Machine_Shop.gasPrice())
			{
				factory->buildAddon(BWAPI::UnitTypes::Terran_Machine_Shop);
			}
			else if (_infoManager.armoryDone() && factory->isIdle() && _infoManager.enemyHasAir() &&
				_infoManager.getVultures().size() + _infoManager.getGoliaths().size() < _infoManager.getTanks().size() * 2 &&
				_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Goliath.mineralPrice() &&
				_self->gas() - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Goliath.gasPrice())
			{
				factory->build(BWAPI::UnitTypes::Terran_Goliath);
			}
			else if (factory->isIdle() && _infoManager.getVultures().size() + _infoManager.getGoliaths().size() < _infoManager.getTanks().size() * 2 &&
				_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Vulture.mineralPrice())
			{
				factory->build(BWAPI::UnitTypes::Terran_Vulture);
			}
			else if (!factory->canBuildAddon() && factory->isIdle() &&
				_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.mineralPrice() &&
				_self->gas() - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode.gasPrice())
			{
				factory->build(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);
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
				_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Control_Tower.mineralPrice() &&
				_self->gas() - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Control_Tower.gasPrice())
			{
				starport->buildAddon(BWAPI::UnitTypes::Terran_Control_Tower);
			}
			else if (starport->getAddon() != NULL)
			{
				if (_infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Dropship) < 1 && (_infoManager.getIslandBases().size() || _infoManager.getOwnedIslandBases().size()) &&
					_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Dropship.mineralPrice() &&
					_self->gas() - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Dropship.gasPrice())
				{
					starport->train(BWAPI::UnitTypes::Terran_Dropship);
				}
				else if (_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility)  && _infoManager.getStrategy() == "SKTerran" &&
					_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Science_Vessel.mineralPrice() &&
					_self->gas() - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Science_Vessel.gasPrice())
				{
					starport->train(BWAPI::UnitTypes::Terran_Science_Vessel);
				}
				else if (_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility) && _infoManager.getStrategy() == "Mech" &&
					_infoManager.getVessels().size() < 2 &&
					_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::UnitTypes::Terran_Science_Vessel.mineralPrice() &&
					_self->gas() - _infoManager.getReservedGas() >= BWAPI::UnitTypes::Terran_Science_Vessel.gasPrice())
				{
					starport->train(BWAPI::UnitTypes::Terran_Science_Vessel);
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
		if (academy->exists() && academy->isIdle() && _infoManager.getStrategy() == "SKTerran")
		{
			if (!_self->hasResearched(BWAPI::TechTypes::Stim_Packs) &&
				_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::TechTypes::Stim_Packs.mineralPrice() &&
				_self->gas() - _infoManager.getReservedGas() >= BWAPI::TechTypes::Stim_Packs.gasPrice())
			{
				academy->research(BWAPI::TechTypes::Stim_Packs);
			}
			else if (_self->hasResearched(BWAPI::TechTypes::Stim_Packs) && !_self->getUpgradeLevel(BWAPI::UpgradeTypes::U_238_Shells) &&
				_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::UpgradeTypes::U_238_Shells.mineralPrice() &&
				_self->gas() - _infoManager.getReservedGas() >= BWAPI::UpgradeTypes::U_238_Shells.gasPrice())
			{
				academy->upgrade(BWAPI::UpgradeTypes::U_238_Shells);
			}
		}
	}
	for (auto & engibay : _infoManager.getEngibays())
	{
		if (engibay->exists() && engibay->isIdle() && _infoManager.getStrategy() == "SKTerran")
		{
			int infantryWeapons = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Infantry_Weapons);
			int infantryArmor = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Infantry_Armor);

			if (infantryWeapons < 3 && !_self->isUpgrading(BWAPI::UpgradeTypes::Terran_Infantry_Weapons) &&
				_self->minerals() - _infoManager.getReservedMinerals() >= UpgradeTypes::Terran_Infantry_Weapons.mineralPrice(infantryWeapons + 1) &&
				_self->gas() - _infoManager.getReservedGas() >= UpgradeTypes::Terran_Infantry_Weapons.gasPrice(infantryWeapons + 1) &&
				(infantryWeapons == 0 || (infantryWeapons > 0 && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility))))
			{
				engibay->upgrade(BWAPI::UpgradeTypes::Terran_Infantry_Weapons);
			}
			else if (infantryArmor < 3 && !_self->isUpgrading(BWAPI::UpgradeTypes::Terran_Infantry_Armor) &&
				_self->minerals() - _infoManager.getReservedMinerals() >= UpgradeTypes::Terran_Infantry_Armor.mineralPrice(infantryArmor + 1) &&
				_self->gas() - _infoManager.getReservedGas() >= UpgradeTypes::Terran_Infantry_Armor.gasPrice(infantryArmor + 1) &&
				(infantryArmor == 0 || (infantryArmor > 0 && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility))))
			{
				engibay->upgrade(BWAPI::UpgradeTypes::Terran_Infantry_Armor);
			}
		}
		else if (engibay->exists() && _infoManager.getStrategy() == "Mech")
		{
			if (!engibay->isLifted() && engibay->isIdle())
				engibay->lift();
		}
	}

	for (auto & armory : _infoManager.getArmories())
	{
		if (armory->exists() && armory->isIdle() && _infoManager.getStrategy() == "Mech")
		{
			int mechWeapons = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons);
			int mechArmor = _self->getUpgradeLevel(BWAPI::UpgradeTypes::Terran_Vehicle_Plating);

			if (mechWeapons < 3 && !_self->isUpgrading(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons) &&
				_self->minerals() - _infoManager.getReservedMinerals() >= UpgradeTypes::Terran_Vehicle_Weapons.mineralPrice(mechWeapons + 1) &&
				_self->gas() - _infoManager.getReservedGas() >= UpgradeTypes::Terran_Vehicle_Weapons.gasPrice(mechWeapons + 1) &&
				(mechWeapons == 0 || (mechWeapons > 0 && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility))))
			{
				armory->upgrade(BWAPI::UpgradeTypes::Terran_Vehicle_Weapons);
			}
			else if (mechArmor < 3 && !_self->isUpgrading(BWAPI::UpgradeTypes::Terran_Vehicle_Plating) &&
				_self->minerals() - _infoManager.getReservedMinerals() >= UpgradeTypes::Terran_Vehicle_Plating.mineralPrice(mechArmor + 1) &&
				_self->gas() - _infoManager.getReservedGas() >= UpgradeTypes::Terran_Vehicle_Plating.gasPrice(mechArmor + 1) &&
				(mechArmor == 0 || (mechArmor > 0 && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Science_Facility))))
			{
				armory->upgrade(BWAPI::UpgradeTypes::Terran_Vehicle_Plating);
			}
		}
	}

	for (auto & scienceFacility : _infoManager.getScience())
	{
		if (scienceFacility->exists() && scienceFacility->isIdle())
		{
			if (!_self->hasResearched(BWAPI::TechTypes::Irradiate) && !_self->isResearching(BWAPI::TechTypes::Irradiate) &&
				_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::TechTypes::Irradiate.mineralPrice() &&
				_self->gas() - _infoManager.getReservedGas() >= BWAPI::TechTypes::Irradiate.gasPrice())
			{
				scienceFacility->research(BWAPI::TechTypes::Irradiate);
			}
		}
	}

	for (auto & machineShop : _infoManager.getMachineShops())
	{
		if (machineShop->exists() && machineShop->isIdle())
		{
			if (!_self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode) && !_self->isResearching(BWAPI::TechTypes::Tank_Siege_Mode) &&
				_self->minerals() - _infoManager.getReservedMinerals() >= BWAPI::TechTypes::Tank_Siege_Mode.mineralPrice() &&
				_self->gas() - _infoManager.getReservedGas() >= BWAPI::TechTypes::Tank_Siege_Mode.gasPrice())
			{
				machineShop->research(BWAPI::TechTypes::Tank_Siege_Mode);
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
	int producerSize = _infoManager.getBarracks().size() + _infoManager.getFactories().size() + _infoManager.getStarports().size() + _infoManager.getCommandCenters().size();

	if (WorkerManager::Instance().checkSupplyConstruction(producerSize, _infoManager.getReservedMinerals()))
	{
		BWAPI::TilePosition targetSupplyLocation = _buildingPlacer.getSupplyLocation(BWAPI::UnitTypes::Terran_Supply_Depot, _infoManager);

		WorkerManager::Instance().supplyConstruction(_infoManager.getWorkers(), targetSupplyLocation, _infoManager.getReservedMinerals());
	}
}

void CreationManager::checkQueue(BWAPI::Unit unit, std::list<BWAPI::UnitType>& queue, int &reservedMinerals, int &reservedGas)
{
	for (std::map<BWAPI::UnitType, int>::iterator it = _constructionQueue.begin(); it != _constructionQueue.end(); ++it)
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
			_constructionQueue.erase(it);
			reservedMinerals = reservedMinerals - unit->getType().mineralPrice();
			reservedGas = reservedGas - unit->getType().gasPrice();
		}
	}
}

CreationManager & CreationManager::Instance()
{
	static CreationManager instance;
	return instance;
}