//////////////////////////////////////////////////////////////////////////
//
// This file is part of the BWEM Library.
// BWEM is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2015, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "base.h"
#include "graph.h"
#include "mapImpl.h"
#include "neutral.h"
#include "bwapiExt.h"


using namespace BWAPI;
using namespace BWAPI::UnitTypes::Enum;
namespace { auto & bw = Broodwar; }

using namespace std;


namespace BWEM {

using namespace detail;
using namespace BWAPI_ext;


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Base
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Base::Base(Area * pArea, const TilePosition & location, const vector<Ressource *> & AssignedRessources, const vector<Mineral *> & BlockingMinerals)
	: m_pArea(pArea),
	m_pMap(pArea->GetMap()),
	m_location(location),
	m_center(Position(location) + Position(UnitType(Terran_Command_Center).tileSize()) / 2),
	m_BlockingMinerals(BlockingMinerals)
{
	bwem_assert(!AssignedRessources.empty());

	/*for (Ressource * r : AssignedRessources)
		if		(Mineral * m = r->IsMineral())	m_Minerals.push_back(m);
		else if (Geyser * g = r->IsGeyser())		m_Geysers.push_back(g);*/
	baseRefinery = NULL;
	int count = 0;
	int countTwo = 0;

	for (Ressource * r : AssignedRessources)
		if (Mineral * m = r->IsMineral())
		{
			m_Minerals.push_back(m);
			// set up a match between minerals and their neutral counterparts
			for (auto unit : BWAPI::Broodwar->getNeutralUnits())
			{
				if (unit->getType().isMineralField() && unit->getInitialPosition() == m->Pos())
				{
					mineralUnit.insert(std::pair<Mineral *, BWAPI::Unit>(m, unit));
				}
			}
		}
		else if (Geyser * g = r->IsGeyser())
		{
			m_Geysers.push_back(g);
		}

	for (auto unit : Broodwar->self()->getUnits())
	{
		if (!unit)
			continue;

		if (unit->getType().isResourceDepot() && (abs(unit->getPosition().x - m_center.x) <= 128 && abs(unit->getPosition().y - m_center.y) <= 128))
		{
			baseCommandCenter = unit;
		}
	}

	turrets.clear();
}


Base::Base(const Base & Other)
	: m_pMap(Other.m_pMap), m_pArea(Other.m_pArea)
{
	bwem_assert(false);
}


void Base::SetStartingLocation(const TilePosition & actualLocation)
{
	m_starting = true;
	m_location = actualLocation;
	m_center = Position(actualLocation) + Position(UnitType(Terran_Command_Center).tileSize()) / 2;
}


void Base::OnMineralDestroyed(const Mineral * pMineral)
{
	if (!pMineral)
		return;

	bwem_assert(pMineral);

	auto iMineral = find(m_Minerals.begin(), m_Minerals.end(), pMineral);
	if (iMineral != m_Minerals.end())
		fast_erase(m_Minerals, distance(m_Minerals.begin(), iMineral));

	iMineral = find(m_BlockingMinerals.begin(), m_BlockingMinerals.end(), pMineral);
	if (iMineral != m_BlockingMinerals.end())
		fast_erase(m_BlockingMinerals, distance(m_BlockingMinerals.begin(), iMineral));

	for (auto assignments : mineral_Assignments)
	{
		if (assignments.first == pMineral)
		{
			mineral_Assignments.erase(assignments.first);
			break;
		}
	}

	for (auto mineral : mineralUnit)
	{
		if (mineral.first == pMineral)
		{
			mineralUnit.erase(mineral.first);
			break;
		}
	}
}

// Added for insanitybot
void Base::updateHishestNumWorkersOnPatch()
{
	if (mineral_Assignments.size() != mineralUnit.size())
		return;

	
	for (auto assignment : mineral_Assignments)
	{
		if (assignment.second.size() < highestNumWorkersOnPatch)
		{
			return;
		}
	}

	if (highestNumWorkersOnPatch + 1 > 3)
		highestNumWorkersOnPatch = 3;
	else
		highestNumWorkersOnPatch += 1;

	
}

Mineral * Base::getDestroyedMineral(BWAPI::Unit patch)
{
	for (auto mineral : mineralUnit)
	{
		if (mineral.second->getInitialPosition() == patch->getInitialPosition())
		{
			return mineral.first;
		}
	}
	return nullptr;
}

void Base::onUnitDestroy(BWAPI::Unit unit)
{
	for (auto & assignment : mineral_Assignments)
	{
		if (assignment.second.contains(unit))
		{
			assignment.second.erase(unit);
			return;
		}
	}

	for (auto & assignment : refinery_Assignments)
	{
		if (assignment.second.contains(unit))
		{
			assignment.second.erase(unit);
			return;
		}
	}
}

void Base::clearAssignmentList()
{
	mineral_Assignments.clear();
	refinery_Assignments.clear();
}

void Base::clearGasAssignment()
{
	for (auto & assignment : refinery_Assignments)
	{
		for (auto & worker : assignment.second)
		{
			if (!worker || !worker->exists())
				continue;

			worker->stop();
		}
	}

	refinery_Assignments.clear();
}

void Base::cleanUpZombieTasks()
{
	for (auto & assignment : mineral_Assignments)
	{
		for (BWAPI::Unitset::iterator worker = assignment.second.begin(); worker != assignment.second.end();)
		{
			if (!(*worker) || !(*worker)->exists())
			{
				worker = assignment.second.erase(worker);
			}
			else
			{
				worker++;
			}
		}
	}

	for (auto & assignment : refinery_Assignments)
	{
		for (BWAPI::Unitset::iterator worker = assignment.second.begin(); worker != assignment.second.end();)
		{
			if (!(*worker) || !(*worker)->exists())
			{
				worker = assignment.second.erase(worker);
			}
			else
			{
				worker++;
			}
		}
	}
}

void Base::setBaseRefinery(BWAPI::Unit refinery)
{
	if (refinery)
	{
		baseRefinery = refinery;
		refinery_Assignments.clear();
	}
	else
	{
		refinery_Assignments.clear();

		baseRefinery = NULL;

		return;
	}

	if (baseRefinery->isConstructing())
	{
		BWAPI::Unit worker = baseRefinery->getBuildUnit();

		for (auto & assignment : mineral_Assignments)
		{
			if (assignment.second.contains(worker))
			{
				assignment.second.erase(worker);
				break;
			}
		}

		BWAPI::Unitset insertNew;
		insertNew.insert(worker);
		refinery_Assignments.insert(std::pair<BWAPI::Unit, BWAPI::Unitset>(baseRefinery, insertNew));
	}
}

void Base::setbaseCommandCenter(BWAPI::Unit commandCenter)
{
	baseCommandCenter = commandCenter;
}

bool Base::isGasWorker(BWAPI::Unit worker)
{
	for (auto assignment : refinery_Assignments)
	{
		if (assignment.second.contains(worker))
		{
			return true;
		}
	}

	return false;
}

bool Base::onTurretDestroy()
{
	std::vector<BWAPI::Unit> turretsToDelete;

	for (const auto& turret : turrets)
	{
		if (!turret || !turret->exists())
		{
			turretsToDelete.push_back(turret);
		}
	}

	for (const auto& turret : turretsToDelete)
	{
		turrets.erase(turret);
	}

	return !turretsToDelete.empty();
}

int Base::numWorkersWantedHere()
{
	int number = (mineralUnit.size() * 2.5);
	if (m_Geysers.size())
		number += 3;
	return number;
}

int Base::getNumMineralWorkers()
{
	int count = 0;
	for (auto assignment : mineral_Assignments)
	{
		count += assignment.second.size();
	}
	return count;
}

int Base::getNumGasWorkers()
{
	int count = 0;
	for (auto assignment : refinery_Assignments)
	{
		count += assignment.second.size();
	}
	return count;
}

int Base::getRemainingMinerals()
{
	int ammount = 0;
	for (auto mineral : m_Minerals)
	{
		if (mineral)
			ammount += mineral->Amount();
	}

	return ammount;
}

void Base::checkAssignment(BWAPI::Unit worker, std::map<BWAPI::Position, BWEM::Base *>& _ownedBases, BWEM::Base* & assignedBase)
{
	if (!worker || !worker->exists())
		return;

	// Check if they're a gas worker
	if (refinery_Assignments.size())
	{
		for (auto assignment : refinery_Assignments)
		{
			if (assignment.second.size() && assignment.second.contains(worker))
			{
				if (!baseCommandCenter->isBeingConstructed() && baseRefinery->exists() && !baseRefinery->isBeingConstructed())
				{
					worker->gather(assignment.first);
					return;
				}
				else
				{
					worker->move(baseCommandCenter->getPosition());
					return;
				}
			}
		}
	}

	// prioritize assigning gas workers if we don't have too much
	if (baseHasRefinery() && !baseRefinery->isBeingConstructed() && 
		3 > getNumGasWorkers())
	{
		assignGasWorkers(worker);
		worker->move(baseRefinery->getPosition());
		return;
	}


	// Check if they're a mineral worker
	if (mineral_Assignments.size())
	{
		for (auto assignment : mineral_Assignments)
		{
			if (assignment.second.contains(worker))
			{
				for (auto patch : mineralUnit)
				{
					if (patch.first == assignment.first)
					{
						if (!baseCommandCenter->isBeingConstructed())
						{
							worker->gather(patch.second);
							return;
						}
						else
						{
							worker->move(baseCommandCenter->getPosition());
							return;
						}
					}
				}
			}
		}
	}
	

	// If we have open assignments here, assign the worker to mine minerals
	if (getNumMineralWorkers() + getNumGasWorkers() < numWorkersWantedHere())
		assignMineralWorkers(worker);
	else
	{
		// If not, see if we have another base to send them to
		for (auto base : _ownedBases)
		{
			if (base.second->getNumMineralWorkers() + base.second->getNumGasWorkers() < base.second->numWorkersWantedHere())
			{
				assignedBase = base.second;
				worker->move(base.first);
				return;
			}
		}
	}

	/*// If we're out of everything else, go harvest minerals somewhere
	Unit closestMineral = nullptr;

	// find the closest mineral
	for (auto allMinerals : BWAPI::Broodwar->getStaticMinerals()) {
		if (allMinerals && allMinerals->exists() &&
			allMinerals->getType().isMineralField()) {
			if (closestMineral == nullptr
				|| worker->getDistance(allMinerals) < worker->getDistance(closestMineral)) {
				closestMineral = allMinerals;
			}
		}
	}

	// if a mineral patch was found, send the worker to gather it
	if (closestMineral != nullptr && !worker->isGatheringMinerals() && 
		!worker->isMoving()) {
		worker->gather(closestMineral, false);
	}*/
}

bool Base::miningAssignmentExists(BWAPI::Unit worker)
{
	for (auto & assignment : mineral_Assignments)
	{
		if (assignment.second.contains(worker))
		{
			return true;
		}
	}

	for (auto & assignment : refinery_Assignments)
	{
		if (assignment.second.contains(worker))
		{
			return true;
		}
	}
	return false;
}

void Base::removeAssignment(BWAPI::Unit worker)
{
	for (auto & assignment : mineral_Assignments)
	{
		for (auto & mineralWorker : assignment.second)
		{
			if (mineralWorker == worker)
			{
				assignment.second.erase(mineralWorker);
				return;
			}
		}
	}

	for (auto & assignment : refinery_Assignments)
	{
		for (auto & gasWorker : assignment.second)
		{
			if (gasWorker == worker)
			{
				assignment.second.erase(gasWorker);
				return;
			}
		}
	}
}

// assignMineralWorkers will hopefully assign workers to mineral patches
// TODO: This could be a bit more optimized
void Base::assignMineralWorkers(BWAPI::Unit worker)
{
	updateHishestNumWorkersOnPatch();
	for (auto mineral : m_Minerals)
	{
		bool found = false;
		for (auto & assignment : mineral_Assignments)
		{
			// Check if the mineral is already within our mapped assignments
			if (assignment.first == mineral)
			{
				//When found, check that we should add a worker to this patch
				if (assignment.second.size() < highestNumWorkersOnPatch)
				{
					// If we do want to add a worker here, insert it and skip out of the loop
					assignment.second.insert(worker);
					return;
				}

				// No point in looping through the rest of the minerals once we've found it
				found = true;
				break;
			}
		}

		// If it is not, insert the new mineral and new worker set
		if (!found)
		{
			BWAPI::Unitset insertNew;
			insertNew.insert(worker);
			mineral_Assignments.insert(std::pair<Mineral *, BWAPI::Unitset>(mineral, insertNew));
			return;
		}
	}
}

void Base::assignGasWorkers(BWAPI::Unit worker)
{
	if (refinery_Assignments.size())
	{
		for (auto & assignment : refinery_Assignments)
		{
			if (assignment.second.size() < 3)
			{
				assignment.second.insert(worker);
				return;
			}
		}
	}
	else
	{
		BWAPI::Unitset insertNew;
		insertNew.insert(worker);
		refinery_Assignments.insert(std::pair<BWAPI::Unit, BWAPI::Unitset>(baseRefinery, insertNew));
	}
}

// End insanitybot section
	
} // namespace BWEM



