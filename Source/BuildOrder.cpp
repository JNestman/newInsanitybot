#include "BuildOrder.h"
#include "InformationManager.h"

namespace { auto & theMap = BWEM::Map::Instance(); }

using namespace insanitybot;

insanitybot::BuildOrder::BuildOrder()
	: _self(BWAPI::Broodwar->self())
	, _enemy(BWAPI::Broodwar->enemy())
{

}

void insanitybot::BuildOrder::initialize(std::string strategy)
{
	_initialStrategy = strategy;
}

/**************************************************
 * a very common pattern in TvZ is this:
 * 1 barracks
 * expand
 * optional 2nd barracks
 * academy and ebay (then stim and +1 weapons)
 * 3 barracks
 * factory (unused but needed for starport)
 * 5 barracks
 * starport
 * science facility
 * 2nd starport
 * 7 barracks
 * produce marine+medic+vessel off 7 barracks+2 port until main is about to run out
 * or in TvP, going mech, on two bases:
 * - macro play would be 3 fac + upgrades + 3rd base
 * - pressure into transition would be 4 fac
 * - all-in aggression would be 5 or 6 fac
 * TvT looks similar but i'm less familiar
 * 5 fac still being very aggressive
 * if memes = nuke you can absolutely do that with the pattern i outlined above -- just swap the 2nd starport for nuke silo
***************************************************/

/**************************************************
 * Fast expand, M-M + Vessel into aggressive play
***************************************************/
void insanitybot::BuildOrder::SKTerran(InformationManager & _infoManager)
{
	std::list<BWAPI::UnitType> & _queue = _infoManager.getQueue();

	int numRaxFinished =		_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);
	int numRaxTotal =			_infoManager.getBarracks().size();
	int numFactoryFinished =	_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Factory);
	int numFactoryTotal =		_infoManager.getFactories().size();
	int numStarportFinished =	_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Starport);
	int numStarportTotal =		_infoManager.getStarports().size();
	int numEngiBaysFinished =	_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Engineering_Bay);
	int numOwnedBases =			_infoManager.getOwnedBases().size();
	int numMarines =			_infoManager.getMarines().size();
	bool hasAcademy =			_infoManager.getAcademy().size();
	bool hasScience =			_infoManager.getScience().size();

	if (!_infoManager.hasInitialBarracks())
	{
		if (_self->minerals() > 100 && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_infoManager.setInitialBarracks(true);
		}
	}
	else
	{
		// 2nd barracks
		if (numRaxFinished <= 1 && (numOwnedBases >= 2 || _self->deadUnitCount(BWAPI::UnitTypes::Terran_Command_Center) > 1) && numRaxTotal <= 1
			&& numMarines > 2)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		// 3rd rax, academy, and engibay
		if (numRaxFinished == 2 && numRaxTotal == 2 && (numMarines > 8 || _self->minerals() > 300))
		{
			if (!hasAcademy)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
			}
			if (_infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Engineering_Bay) < 1)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
			}

			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		// fact for port
		if (hasAcademy && numFactoryTotal < 1 && _infoManager.getRefineries().size() &&
			(numMarines > 16 || _self->minerals() > 300))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}

		// 5 rax and port
		if (numFactoryFinished >= 1 && (numMarines > 24 || _self->minerals() > 300) && numStarportTotal == 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}

		// science facility
		if (numStarportFinished >= 1 && !hasScience && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Science_Facility) < 1)
		{
			if (_infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Engineering_Bay) < 2)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
			}
			_queue.push_back(BWAPI::UnitTypes::Terran_Science_Facility);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Science_Facility.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Science_Facility.gasPrice());
			// 2nd port
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}
		else if (hasScience && numRaxTotal < 7)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		// 1st expansion
		if (numMarines > 0 && numOwnedBases < 2)
		{
			if (_infoManager.getBunkers().size() && !_infoManager.closeEnough((*_infoManager.getBunkers().begin())->getPosition(), BWAPI::Position(_infoManager.getNatBunkerPos())))
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
			}
			else if (!_infoManager.getBunkers().size())
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
			}
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}

		// Island expansions
		if (numOwnedBases >= 2 && _infoManager.shouldExpand() && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Dropship) && _infoManager.getIslandBases().size() &&
			!_infoManager.shouldIslandExpand() && _infoManager.getIslandBuilder() == NULL && !_infoManager.getOwnedIslandBases().size())
		{
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
			_infoManager.setIslandExpand(true);
		}
		// all other expansions
		else if (numOwnedBases >= 2 && _infoManager.shouldExpand() && _infoManager.getReservedMinerals() < 400)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}
	}

	if (hasAcademy && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Refinery) < _infoManager.numGeyserBases())
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
	}
}

/**************************************************
 * Fast expand, SKTerran with nukes and ghosts
***************************************************/
void insanitybot::BuildOrder::Nuke(InformationManager & _infoManager)
{
	std::list<BWAPI::UnitType> & _queue = _infoManager.getQueue();

	int numRaxFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);
	int numRaxTotal = _infoManager.getBarracks().size();
	int numFactoryFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Factory);
	int numFactoryTotal = _infoManager.getFactories().size();
	int numStarportFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Starport);
	int numStarportTotal = _infoManager.getStarports().size();
	int numEngiBaysFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Engineering_Bay);
	int numOwnedBases = _infoManager.getOwnedBases().size();
	int numMarines = _infoManager.getMarines().size();
	bool hasAcademy = _infoManager.getAcademy().size();
	bool hasScience = _infoManager.getScience().size();

	if (!_infoManager.hasInitialBarracks())
	{
		if (_self->minerals() > 100 && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_infoManager.setInitialBarracks(true);
		}
	}
	else
	{
		// 2nd barracks
		if (numRaxFinished <= 1 && (numOwnedBases >= 2 || _self->deadUnitCount(BWAPI::UnitTypes::Terran_Command_Center) > 1) && numRaxTotal <= 1
			&& numMarines > 2)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		// 3rd rax, academy, and engibay
		if (numRaxFinished == 2 && numRaxTotal == 2 && (numMarines > 8 || _self->minerals() > 300))
		{
			if (!hasAcademy)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
			}
			if (_infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Engineering_Bay) < 1)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
			}

			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		// fact for port
		if (hasAcademy && numFactoryTotal < 1 && _infoManager.getRefineries().size() &&
			(numMarines > 16 || _self->minerals() > 300))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}

		// 5 rax and port
		if (numFactoryFinished >= 1 && (numMarines > 24 || _self->minerals() > 300) && numStarportTotal == 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}

		// science facility
		if (numStarportFinished >= 1 && !hasScience && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Science_Facility) < 1)
		{
			if (_infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Engineering_Bay) < 2)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
			}
			_queue.push_back(BWAPI::UnitTypes::Terran_Science_Facility);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Science_Facility.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Science_Facility.gasPrice());
		}
		else if (hasScience && numRaxTotal < 7)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		if (_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Nuclear_Missile) && numFactoryFinished >= 1 && numStarportTotal <= 1)
		{
			// 2nd port
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}

		// 1st expansion
		if (_infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Marine) > 0 && numOwnedBases < 2)
		{
			if (_infoManager.getBunkers().size() && !_infoManager.closeEnough((*_infoManager.getBunkers().begin())->getPosition(), BWAPI::Position(_infoManager.getNatBunkerPos())))
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
			}
			else if (!_infoManager.getBunkers().size())
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
			}
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}

		// Island expansions
		if (numOwnedBases >= 2 && _infoManager.shouldExpand() && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Dropship) && _infoManager.getIslandBases().size() &&
			!_infoManager.shouldIslandExpand() && _infoManager.getIslandBuilder() == NULL && !_infoManager.getOwnedIslandBases().size())
		{
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
			_infoManager.setIslandExpand(true);
		}
		// all other expansions
		else if (numOwnedBases >= 2 && _infoManager.shouldExpand() && _infoManager.getReservedMinerals() < 400)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}
	}

	bool trainingNuke = false;

	for (auto addon : _infoManager.getAddons())
	{
		if (addon->getType() == BWAPI::UnitTypes::Terran_Nuclear_Silo && !addon->isIdle())
		{
			trainingNuke = true;
			break;
		}
	}

	if (_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Nuclear_Silo) && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Nuclear_Missile) < 1 && !trainingNuke)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Nuclear_Missile);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Nuclear_Missile.mineralPrice());
		_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Nuclear_Missile.gasPrice());
	}

	if (hasAcademy && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Refinery) - _infoManager.getOwnedIslandBases().size() < _infoManager.numGeyserBases())
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
	}

	if (!hasAcademy && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Academy) > 0)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
	}
}

/**************************************************
 * Seige Expand into Mech
***************************************************/
void insanitybot::BuildOrder::Mech(InformationManager & _infoManager)
{
	std::list<BWAPI::UnitType> & _queue = _infoManager.getQueue();

	int numRaxFinished =		_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);
	int numRaxTotal =			_infoManager.getBarracks().size();
	int numFactoryFinished =	_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Factory);
	int numFactoryTotal =		_infoManager.getFactories().size();
	int numStarportFinished =	_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Starport);
	int numStarportTotal =		_infoManager.getStarports().size();
	int numEngiBaysFinished =	_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Engineering_Bay);
	int numEngiBaysTotal =		_infoManager.getEngibays().size();
	int numArmoriesTotal =		_infoManager.getArmories().size();
	int numMachineShopTotal =	_infoManager.getMachineShops().size();
	int numOwnedBases =			_infoManager.getOwnedBases().size();
	int numMarines =			_infoManager.getMarines().size();
	bool hasAcademy =			_infoManager.getAcademy().size();
	bool hasScience =			_infoManager.getScience().size();

	if (!_infoManager.hasInitialBarracks())
	{
		if (_self->minerals() > 100 && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_infoManager.setInitialBarracks(true);
		}
	}
	else
	{
		if (numRaxTotal == 0 && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Barracks) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		if (numEngiBaysTotal == 0 && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
		}

		// first factory
		if (_self->gas() >= 100 && numFactoryTotal == 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}
		// second factory, engibay, academy
		else if (numFactoryFinished == 1 && numFactoryTotal == 1 && numMachineShopTotal &&
			(_self->isResearching(BWAPI::TechTypes::Tank_Siege_Mode) || _self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode)) &&
			numOwnedBases > 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
			if (numEngiBaysTotal < 1)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
			}

			if (!hasAcademy)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
			}
		}
		// third factory and armory
		else if (numFactoryFinished == 2 && numFactoryTotal == 2 && (_infoManager.getTanks().size() > 3 || _self->minerals() > 300))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
			if (numArmoriesTotal < 2)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Armory);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Armory.mineralPrice());
				_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Armory.gasPrice());
			}
		}
		// forth and fifth factory
		else if (numFactoryFinished < 5 && numFactoryTotal < 5 && _infoManager.getTanks().size() > 5)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
			if (numArmoriesTotal < 2)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Armory);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Armory.mineralPrice());
				_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Armory.gasPrice());
			}
		}

		// Starport
		if (numFactoryFinished >= 3 && numStarportTotal < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}

		// science facility
		if (numStarportFinished >= 1 && !hasScience && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Science_Facility) < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Science_Facility);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Science_Facility.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Science_Facility.gasPrice());
		}


		// 1st expansion
		if ((numMarines > 0 || _self->deadUnitCount(BWAPI::UnitTypes::Terran_Marine) == 4) &&
			numOwnedBases < 2 && !_infoManager.shouldHaveDefenseSquad(false) &&
			(_self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode) || _self->isResearching(BWAPI::TechTypes::Tank_Siege_Mode)))
		{
			if (_self->deadUnitCount(BWAPI::UnitTypes::Terran_Bunker) < 1 && !_infoManager.getBunkers().size())
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
			}


			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}
		// Island expansions
		else if (numOwnedBases >= 2 && _infoManager.shouldExpand() && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Dropship) && _infoManager.getIslandBases().size() &&
			!_infoManager.shouldIslandExpand() && _infoManager.getIslandBuilder() == NULL && !_infoManager.getIslandBases().size())
		{
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
			_infoManager.setIslandExpand(true);
		} // Other Expansions
		else if (numOwnedBases >= 2 && _infoManager.shouldExpand() && (_self->minerals() > 800 || BWAPI::Broodwar->getFrameCount() > 20000) &&
			!_infoManager.shouldIslandExpand())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}

		// Factory up to 7
		if (numFactoryTotal < 7 && numFactoryFinished >= 5 && numOwnedBases > 2 && _self->minerals() > 300 && _self->gas() > 150)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}

		if (!hasAcademy && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Academy) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
		}

		// initial bunker
		if (numRaxFinished && numFactoryTotal &&
			_self->deadUnitCount(BWAPI::UnitTypes::Terran_Bunker) < 1 && !_infoManager.getBunkers().size())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
		}

		// Geysers
		if (numRaxTotal && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Refinery) < _infoManager.numGeyserBases() &&
			_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center) >= _infoManager.numGeyserBases())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
		}
	}
}

/**************************************************
 * Fast Expand into Mech, a bit more aggressive since
 * T bots tend to be more passive early on
***************************************************/
void insanitybot::BuildOrder::MechVT(InformationManager & _infoManager)
{
	std::list<BWAPI::UnitType> & _queue = _infoManager.getQueue();

	int numRaxFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);
	int numRaxTotal = _infoManager.getBarracks().size();
	int numFactoryFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Factory);
	int numFactoryTotal = _infoManager.getFactories().size();
	int numStarportFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Starport);
	int numStarportTotal = _infoManager.getStarports().size();
	int numEngiBaysFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Engineering_Bay);
	int numEngiBaysTotal = _infoManager.getEngibays().size();
	int numArmoriesTotal = _infoManager.getArmories().size();
	int numMachineShopTotal = _infoManager.getMachineShops().size();
	int numOwnedBases = _infoManager.getOwnedBases().size();
	int numMarines = _infoManager.getMarines().size();
	bool hasAcademy = _infoManager.getAcademy().size();
	bool hasScience = _infoManager.getScience().size();

	if (!_infoManager.hasInitialBarracks())
	{
		if (_self->minerals() > 100 && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_infoManager.setInitialBarracks(true);
		}
	}
	else
	{
		if (numRaxTotal == 0 && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Barracks) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		if (numEngiBaysTotal == 0 && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
		}

		// first factory
		if (_self->gas() >= 100 && numFactoryTotal == 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}
		// second factory, engibay, academy
		else if (numFactoryFinished <= 1 && numFactoryTotal == 1 &&
			_self->minerals() > 200 && _self->gas()  > 100 &&
			numOwnedBases > 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
			if (numEngiBaysTotal < 1)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
			}

			if (!hasAcademy)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
			}
		}
		// third factory and armory
		else if (numFactoryFinished == 2 && numFactoryTotal == 2 && (_infoManager.getTanks().size() > 3 || _self->minerals() > 300))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
			if (numArmoriesTotal < 2)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Armory);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Armory.mineralPrice());
				_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Armory.gasPrice());
			}
		}
		// forth and fifth factory
		else if (numFactoryFinished < 5 && numFactoryTotal < 5 && _infoManager.getTanks().size() > 5)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
			if (numArmoriesTotal < 2)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Armory);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Armory.mineralPrice());
				_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Armory.gasPrice());
			}
		}

		// Starport
		if (numFactoryFinished >= 3 && numStarportTotal < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}

		// science facility
		if (numStarportFinished >= 1 && !hasScience && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Science_Facility) < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Science_Facility);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Science_Facility.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Science_Facility.gasPrice());
		}


		// 1st expansion
		if (numRaxFinished && numOwnedBases < 2 && !_infoManager.shouldHaveDefenseSquad(false))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}
		// Island expansions
		else if (numOwnedBases >= 2 && _infoManager.shouldExpand() && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Dropship) && _infoManager.getIslandBases().size() &&
			!_infoManager.shouldIslandExpand() && _infoManager.getIslandBuilder() == NULL && !_infoManager.getIslandBases().size())
		{
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
			_infoManager.setIslandExpand(true);
		} // Other Expansions
		else if (numOwnedBases >= 2 && _infoManager.shouldExpand() && (_self->minerals() > 800 || BWAPI::Broodwar->getFrameCount() > 20000) &&
			!_infoManager.shouldIslandExpand())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}

		// Factory up to 7
		if (numFactoryTotal < 7 && numFactoryFinished >= 5 && numOwnedBases > 2 && _self->minerals() > 300 && _self->gas() > 150)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}

		if (!hasAcademy && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Academy) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
		}

		// initial bunker
		if (numRaxFinished && numMarines &&
			numOwnedBases > 1 &&
			_self->deadUnitCount(BWAPI::UnitTypes::Terran_Bunker) < 1 && !_infoManager.getBunkers().size())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
		}

		// Geysers
		if (numRaxTotal && numOwnedBases > 1 && 
			_infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Refinery) < _infoManager.numGeyserBases() &&
			_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center) >= _infoManager.numGeyserBases() &&
			_infoManager.getTanks().size() > 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
		}
		else if (numRaxFinished && numOwnedBases > 1 && (_infoManager.getBunkers().size() || _self->minerals() >= 200) &&
				_infoManager.getRefineries().size() < 1 && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
		}
	}
}

/**************************************************
 * Fast Expand into BCs
***************************************************/
void insanitybot::BuildOrder::BCMeme(InformationManager & _infoManager)
{
	std::list<BWAPI::UnitType> & _queue = _infoManager.getQueue();

	int numRaxFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);
	int numRaxTotal = _infoManager.getBarracks().size();
	int numFactoryFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Factory);
	int numFactoryTotal = _infoManager.getFactories().size();
	int numStarportFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Starport);
	int numStarportTotal = _infoManager.getStarports().size();
	int numEngiBaysTotal = _infoManager.getEngibays().size();
	int numArmoriesTotal = _infoManager.getArmories().size();
	int numMachineShopTotal = _infoManager.getMachineShops().size();
	int numOwnedBases = _infoManager.getOwnedBases().size();
	int numMarines = _infoManager.getMarines().size();
	bool hasAcademy = _infoManager.getAcademy().size();
	bool hasScience = _infoManager.getScience().size();

	if (!_infoManager.hasInitialBarracks())
	{
		if (_self->minerals() > 100 && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_infoManager.setInitialBarracks(true);
		}
	}
	else
	{
		if (numRaxTotal == 0 && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Barracks) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		if (numEngiBaysTotal == 0 && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
		}

		// first factory
		if (_self->gas() >= 100 && numFactoryTotal == 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}
		// first starport, engibay, academy
		else if (numFactoryFinished == 1 && numStarportTotal == 0 && numMachineShopTotal &&
			(_self->isResearching(BWAPI::TechTypes::Tank_Siege_Mode) || _self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode)) &&
			numOwnedBases > 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());

			if (numEngiBaysTotal < 1)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
			}

			if (!hasAcademy)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
			}
		}
		// second starport
		else if (numStarportFinished == 1 && numStarportTotal == 1 && hasScience &&
			(_infoManager.getTanks().size() > 2 || _self->minerals() > 300))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}
		// third starport
		else if (numStarportFinished < 3 && numStarportTotal < 3 &&
			(_infoManager.getBCs().size() > 2 || (_self->minerals() > 500 && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Physics_Lab))))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}

		// science facility
		if (numStarportFinished >= 1 && !hasScience && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Science_Facility) < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Science_Facility);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Science_Facility.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Science_Facility.gasPrice());
		}

		// Extra factories
		if (numStarportFinished == 3 && numFactoryTotal < 3 && _infoManager.getBCs().size() > 2 && _self->minerals() > 500)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}

		// Armories
		if (numFactoryFinished && _infoManager.getBCs().size() > 4 && _self->gas() > 200 &&
			numArmoriesTotal < 2)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Armory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Armory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Armory.gasPrice());
		}

		// 1st expansion
		if ((numMarines > 0 || _self->deadUnitCount(BWAPI::UnitTypes::Terran_Marine) == 4) &&
			numOwnedBases < 2 && !_infoManager.shouldHaveDefenseSquad(false) &&
			(_self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode) || _self->isResearching(BWAPI::TechTypes::Tank_Siege_Mode)))
		{
			if (_self->deadUnitCount(BWAPI::UnitTypes::Terran_Bunker) < 1 && !_infoManager.getBunkers().size())
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
			}


			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}
		// Island expansions
		else if (numOwnedBases >= 2 && _infoManager.shouldExpand() && _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Dropship) && _infoManager.getIslandBases().size() &&
			!_infoManager.shouldIslandExpand() && _infoManager.getIslandBuilder() == NULL && !_infoManager.getOwnedIslandBases().size())
		{
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
			_infoManager.setIslandExpand(true);
		} // Other Expansions
		else if (numOwnedBases >= 2 && _infoManager.shouldExpand() && (_self->minerals() > 800 || BWAPI::Broodwar->getFrameCount() > 20000) &&
			!_infoManager.shouldIslandExpand())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}

		// initial bunker
		if (numRaxFinished && numFactoryTotal &&
			_self->deadUnitCount(BWAPI::UnitTypes::Terran_Bunker) < 1 && !_infoManager.getBunkers().size())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
		}

		// Geysers
		if (numRaxTotal && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Refinery) < _infoManager.numGeyserBases() &&
			_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center) >= _infoManager.numGeyserBases())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
		}
	}
}

/**************************************************
 * Anti-Rush Builds
***************************************************/
void insanitybot::BuildOrder::EightRaxDef(InformationManager & _infoManager)
{
	std::list<BWAPI::UnitType> & _queue = _infoManager.getQueue();

	int numRaxFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);
	int numRaxTotal = _infoManager.getBarracks().size();
	int numEngiBaysTotal = _infoManager.getEngibays().size();
	int numMarines = _infoManager.getMarines().size();
	int numBunkers = _infoManager.getBunkers().size();
	bool hasAcademy = _infoManager.getAcademy().size();

	if (numMarines > 18)
	{
		_infoManager.setStrategy(_initialStrategy);
		_infoManager.setEnemyRushing(false);
		return;
	}

	if (!_infoManager.hasInitialBarracks() || numRaxTotal < 1)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		_infoManager.setInitialBarracks(true);
	}
	else if (numRaxFinished && numBunkers < 1)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
	}
	else if (numBunkers && numRaxTotal < 2 && (numMarines > 3 || _self->minerals() > 300))
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
	}

	if (numRaxFinished >= 2 && !_infoManager.getRefineries().size())
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
	}
	else if (numRaxFinished && _infoManager.getRefineries().size() && !hasAcademy)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
	}

	if (numRaxFinished >= 2 && !numEngiBaysTotal && _self->minerals() > 200)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
	}
}

// Defensive Mech
void insanitybot::BuildOrder::OneBaseMech(InformationManager & _infoManager)
{
	std::list<BWAPI::UnitType> & _queue = _infoManager.getQueue();

	int numRaxFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);
	int numRaxTotal = _infoManager.getBarracks().size();
	int numFactoryTotal = _infoManager.getFactories().size();
	int numEngiBaysTotal = _infoManager.getEngibays().size();
	int numBunkers = _infoManager.getBunkers().size();
	int numMarines = _infoManager.getMarines().size();
	int numTanks = _infoManager.getTanks().size();
	bool hasAcademy = _infoManager.getAcademy().size();

	if (numTanks > 2)
	{
		if (!hasAcademy)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
		}

		_infoManager.setStrategy(_initialStrategy);
		_infoManager.setEnemyRushing(false);
		return;
	}

	if (!_infoManager.hasInitialBarracks() || numRaxTotal < 1)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		_infoManager.setInitialBarracks(true);
	}
	else if (numRaxFinished && numBunkers + _self->deadUnitCount(BWAPI::UnitTypes::Terran_Bunker) < 1)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
	}
	else if ((numBunkers || _self->deadUnitCount(BWAPI::UnitTypes::Terran_Bunker) > 0) && numRaxTotal && _infoManager.getRefineries().size() < 1)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
	}
	else if (numRaxTotal && _self->gas() > 100 && numFactoryTotal < 2)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
		_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
	}

	if (numTanks && !numEngiBaysTotal)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
	}
}

/**********************************************
* One base all in
***********************************************/
// All in Bio Mech
void insanitybot::BuildOrder::OneFacAllIn(InformationManager & _infoManager)
{
	std::list<BWAPI::UnitType> & _queue = _infoManager.getQueue();

	int numRaxFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);
	int numRaxTotal = _infoManager.getBarracks().size();
	int numFactoryFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Factory);
	int numFactoryTotal = _infoManager.getFactories().size();
	int numStarportFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Starport);
	int numStarportTotal = _infoManager.getStarports().size();
	int numEngiBaysTotal = _infoManager.getEngibays().size();
	int numArmoriesTotal = _infoManager.getArmories().size();
	int numOwnedBases = _infoManager.getOwnedBases().size();
	int numMarines = _infoManager.getMarines().size();
	int numTanks = _infoManager.getTanks().size();
	bool hasAcademy = _infoManager.getAcademy().size();
	bool hasScience = _infoManager.getScience().size();

	bool needDetection = false;

	for (auto unit : _enemy->getUnits())
	{
		if (unit && unit->exists())
		{
			if (unit->isCloaked() || unit->isBurrowed())
			{
				needDetection = true;
				break;
			}
		}
	}

	if (!_infoManager.hasInitialBarracks() && _infoManager.getWorkers().size() >= 8 && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		_infoManager.setInitialBarracks(true);
	}
	else
	{
		if (numMarines && _infoManager.getBunkers().size() + _self->deadUnitCount(BWAPI::UnitTypes::Terran_Bunker) < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
		}

		if (numRaxFinished && _infoManager.getRefineries().size() < 1 && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
		}
		else if (numRaxTotal < 2 && _infoManager.getRefineries().size() && _infoManager.getWorkers().size() > 15 && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0 &&
			(_self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode) || _self->isResearching(BWAPI::TechTypes::Tank_Siege_Mode)))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}
		else if (numRaxFinished < 3 && _self->minerals() > 300 && _self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}
		else if (((numRaxTotal <= 3 && _self->minerals() > 300 && _self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode)) ||
			_infoManager.enemyHasAir()) && !numEngiBaysTotal)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
		}

		if (numRaxFinished && _self->gas() > 75 && numFactoryTotal < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}

		if (numRaxTotal && numFactoryTotal && !hasAcademy && numMarines > 5 &&
			numTanks && (_self->isResearching(BWAPI::TechTypes::Tank_Siege_Mode) || _self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode)))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
		}

		if (numFactoryFinished && _self->minerals() > 500 && _self->gas() > 300 && numArmoriesTotal < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Armory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Armory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Armory.gasPrice());
		}

		// Starport for detection
		if (((numFactoryFinished && numTanks > 6) || needDetection) && !numStarportTotal)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}

		if (numStarportFinished && !hasScience)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Science_Facility);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Science_Facility.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Science_Facility.gasPrice());
		}

		if (_infoManager.shouldExpand())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}

		// Geysers
		if (numRaxFinished && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Refinery) < _infoManager.numGeyserBases() &&
			_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center) >= _infoManager.numGeyserBases() &&
			numOwnedBases > 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
		}

		if (_self->deadUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) + _self->deadUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) > 6)
		{
			_infoManager.setStrategy("Mech");
			_infoManager.setAggression(false);
		}
	}
}

// Limited bio mech all in
void insanitybot::BuildOrder::MechAllIn(InformationManager & _infoManager)
{
	std::list<BWAPI::UnitType> & _queue = _infoManager.getQueue();

	int numRaxFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);
	int numRaxTotal = _infoManager.getBarracks().size();
	int numFactoryFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Factory);
	int numFactoryTotal = _infoManager.getFactories().size();
	int numStarportFinished = _infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Starport);
	int numStarportTotal = _infoManager.getStarports().size();
	int numEngiBaysTotal = _infoManager.getEngibays().size();
	int numMachineshopTotal = _infoManager.getMachineShops().size();
	int numArmoriesTotal = _infoManager.getArmories().size();
	int numOwnedBases = _infoManager.getOwnedBases().size();
	int numMarines = _infoManager.getMarines().size();
	int numTanks = _infoManager.getTanks().size();
	bool hasAcademy = _infoManager.getAcademy().size();
	bool hasScience = _infoManager.getScience().size();

	if (!_infoManager.hasInitialBarracks() && _infoManager.getWorkers().size() >= 8 && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
		_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		_infoManager.setInitialBarracks(true);
	}
	else
	{
		if (numRaxFinished && _infoManager.getRefineries().size() < 1 && _infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
		}
		else if (((numRaxFinished && _self->minerals() > 300 && _self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode)) ||
			_infoManager.enemyHasAir()) && !numEngiBaysTotal)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
		}

		if (numRaxFinished && _self->gas() > 100 && numFactoryTotal < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}
		else if (numRaxFinished && numFactoryTotal < 3 && numFactoryTotal >= 1 &&
			numMachineshopTotal && _infoManager.getVultures().size() > 2)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}

		if (numRaxFinished && numFactoryTotal && !hasAcademy && _infoManager.getVultures().size() > 6 &&
			(_self->isResearching(BWAPI::TechTypes::Spider_Mines) || _self->hasResearched(BWAPI::TechTypes::Spider_Mines)))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
		}

		if (numFactoryTotal && _self->minerals() > 500 && _self->gas() > 300 && numArmoriesTotal < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Armory);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Armory.mineralPrice());
			_infoManager.setReservedGas(_infoManager.getReservedGas() + BWAPI::UnitTypes::Terran_Armory.gasPrice());
		}

		if (_infoManager.shouldExpand() || BWAPI::Broodwar->getFrameCount() > 12000)
		{
			_infoManager.setStrategy("Mech");

			_infoManager.setAggression(false);

			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());

			if (!numEngiBaysTotal)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
				_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
			}

			return;
		}

		if ((_infoManager.getNumTotalUnit(BWAPI::UnitTypes::Terran_Marine) > 0 && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Marine) <= 4) &&
			numRaxFinished &&
			_self->deadUnitCount(BWAPI::UnitTypes::Terran_Bunker) < 1 && !_infoManager.getBunkers().size() &&
			!_infoManager.shouldHaveDefenseSquad(false) && numFactoryTotal)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
			_infoManager.setReservedMinerals(_infoManager.getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
		}
	}
}

/**************************************************
 **************************************************
 * END BUILD ORDER SECTION
 **************************************************
***************************************************/

BuildOrder & BuildOrder::Instance()
{
	static BuildOrder instance;
	return instance;
}