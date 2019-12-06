#include "InformationManager.h"


namespace { auto & theMap = BWEM::Map::Instance(); }

using namespace insanitybot;

insanitybot::InformationManager::InformationManager()
	:	_self(BWAPI::Broodwar->self())
	, _enemy(BWAPI::Broodwar->enemy())
{
}

void InformationManager::initialize()
{
	_buildOrder.initialize();

	_reservedMinerals = 0;
	_reservedGas = 0;

	BWAPI::Position main;
	for (auto &u : BWAPI::Broodwar->self()->getUnits())
	{
		if (u->getType().isResourceDepot())
		{
			//BWAPI::Broodwar << "Main Position at: " << u->getPosition() << std::endl;
			main = u->getPosition();
		}
	}
	for (const BWEM::Area & area : theMap.Areas())
	{
		for (const BWEM::Base & base : area.Bases())
		{
			if (closeEnough(BWAPI::Position(base.Location()), main))
			{
				std::vector<BWEM::Ressource *> assignedResources;
				for (BWEM::Mineral * m : base.Minerals())
				{
					assignedResources.push_back(m);
				}
				for (BWEM::Geyser * g : base.Geysers())
				{
					assignedResources.push_back(g);

				}
				BWEM::Area _homeArea = area;
				_home = new BWEM::Base(&_homeArea, base.Location(), assignedResources, base.BlockingMinerals());

			}
			else
			{
				std::vector<BWEM::Ressource *> assignedResources;
				for (BWEM::Mineral * m : base.Minerals())
				{
					assignedResources.push_back(m);
				}
				for (BWEM::Geyser * g : base.Geysers())
				{
					assignedResources.push_back(g);

				}
				BWEM::Area _otherBaseArea = area;
				_otherBases[BWAPI::Position(base.Location())] = new BWEM::Base(&_otherBaseArea, base.Location(), assignedResources, base.BlockingMinerals());
			}
		}
	}
}

void InformationManager::update()
{
	//Update build order
	_buildOrder.update();

	//Clear the lists of units
	_commandCenters.clear();
	_workers.clear();
	_producers.clear();
	_combatUnits.clear();
	_comSats.clear();
	_addons.clear();

	//Loop through the units and split them into their respective groups
	for (BWAPI::Unit myUnit : _self->getUnits())
	{
		if (myUnit->getType().isWorker())
		{
			_workers.push_back(myUnit);
		}
		else if (myUnit->getType().isBuilding() && myUnit->getType().canProduce()
			&& !myUnit->getType().isResourceDepot() && !myUnit->getType().isAddon())
		{
			_producers.push_back(myUnit);
		}
		else if (myUnit->getType().isAddon())
		{
			if (myUnit->getType() == BWAPI::UnitTypes::Terran_Comsat_Station)
			{
				_comSats.push_back(myUnit);
			}
			else
			{
				_addons.push_back(myUnit);
			}

		}
		else if (myUnit->getType().isResourceDepot())
		{
			_commandCenters.push_back(myUnit);
		}
	}
}

bool insanitybot::InformationManager::closeEnough(BWAPI::Position location1, BWAPI::Position location2)
{
	return abs(location1.x - location2.x) <= 64 && abs(location1.y - location2.y) <= 64;
}


int insanitybot::InformationManager::getNumFinishedUnit(BWAPI::UnitType type)
{
	int count = 0;
	for (auto & unit : _self->getUnits())
	{
		if (unit->getType() == type && unit->isCompleted() && unit->exists())
		{
			count += 1;
		}
	}
	return count;
}

int insanitybot::InformationManager::getNumUnfinishedUnit(BWAPI::UnitType type)
{
	int count = 0;
	for (auto & unit : _self->getUnits())
	{
		if (unit->getType() == type && !unit->isCompleted() && unit->exists())
		{
			count += 1;
		}
	}
	return count;
}

int insanitybot::InformationManager::getNumTotalUnit(BWAPI::UnitType type)
{
	int count = 0;
	for (auto & unit : _self->getUnits())
	{
		if (unit->getType() == type && unit->exists())
		{
			count += 1;
		}
	}
	return count;
}

int insanitybot::InformationManager::numWorkersWanted()
{
	int numWanted = 0;
	std::list<BWAPI::Position> myBases;
	for (auto &u : BWAPI::Broodwar->self()->getUnits())
	{
		if (u->getType().isResourceDepot())
		{
			//BWAPI::Broodwar << "Main Position at: " << u->getPosition() << std::endl;
			myBases.push_back(u->getPosition());
		}
	}
	for (const BWEM::Area & area : theMap.Areas())
		for (const BWEM::Base & base : area.Bases())
		{
			for (auto &b : myBases)
			{
				if (closeEnough(BWAPI::Position(base.Location()), b))
				{
					numWanted += base.Minerals().size() * 3;
					numWanted += base.Geysers().size() * 3;
				}
			}
			
		}
	if (numWanted > 70)
		return 70;
	else if (numWanted < 3)
		return 3;
	else
		return numWanted;
}

InformationManager & InformationManager::Instance()
{
	static InformationManager instance;
	return instance;
}