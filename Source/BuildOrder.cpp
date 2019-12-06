
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
				_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
				_initialBarracks = true;
			}
		}
		else
		{
			int numRaxWanted = InformationManager::Instance().getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center) * 2;
			if (numRaxWanted > 12)
				numRaxWanted = 12;
			if (numRaxWanted > InformationManager::Instance().getNumTotalUnit(BWAPI::UnitTypes::Terran_Barracks))
				_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
		}
		BWAPI::Broodwar->drawTextScreen(200, 40, "Queue: EMPTY");
	}
}
