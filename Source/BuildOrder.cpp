
#include "InformationManager.h"
#include "BuildOrder.h"
#pragma once
using namespace insanitybot;

insanitybot::Build::Build()
	: _self(BWAPI::Broodwar->self())
	, _enemy(BWAPI::Broodwar->enemy())
{
}

void insanitybot::Build::initialize()
{
	_queue.clear();
	_initialBarracks = false;
}

void insanitybot::Build::update()
{
	if (_queue.empty())
	{
		if (!_initialBarracks)
		{
			if (_self->minerals() > 100 && InformationManager::Instance().getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
			{
				BWAPI::Broodwar << "_queue is empty: Initial barracks" << std::endl;
				_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
				InformationManager::Instance().setReservedMinerals(InformationManager::Instance().getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
				_initialBarracks = true;
			}
		}
		else
		{
			int numRaxWanted = InformationManager::Instance().getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center) * 3;
			if (numRaxWanted > InformationManager::Instance().getNumTotalUnit(BWAPI::UnitTypes::Terran_Barracks))
			{
				BWAPI::Broodwar << "_queue is empty: next barracks" << std::endl;
				_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
				InformationManager::Instance().setReservedMinerals(InformationManager::Instance().getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			}
		}
	}
}
