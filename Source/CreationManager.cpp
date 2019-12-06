#include "CreationManager.h"

using namespace insanitybot;

insanitybot::CreationManager::CreationManager()
	: _self(BWAPI::Broodwar->self())
{
}

void CreationManager::update(std::list<BWAPI::Unit> _producers, std::list<BWAPI::Unit> _commandCenters, std::list<BWAPI::Unit> _workers, int numWorkersWanted, std::list<BWAPI::UnitType>& queue)
{
	for (auto & center : _commandCenters)
	{
		if (center->isIdle() && _self->minerals() >= 50 && _workers.size() < numWorkersWanted)
		{
			center->train(center->getType().getRace().getWorker());
		}
	}
	if (!queue.empty())
	{
		for (BWAPI::UnitType unit : queue)
		{
			if (unit.isBuilding() && _self->minerals() >= unit.mineralPrice() && _self->gas() >= unit.gasPrice())
			{
				WorkerManager::Instance().construct(_workers, unit);
				queue.remove(unit);
			}
			else
			{

			}
		}
	}
	for (auto & producer : _producers)
	{
		if (producer->getType() == BWAPI::UnitTypes::Terran_Barracks && producer->isIdle() && _self->minerals() >= 50)
		{
			producer->train(BWAPI::UnitTypes::Terran_Marine);
		}
	}
}