#include "WorkerManager.h"

using namespace insanitybot;

void WorkerManager::update(std::list<BWAPI::Unit> _workers, int numProducers)
{
	for (auto & worker : _workers)
	{
		if (worker->isIdle())
		{
			// Order workers carrying a resource to return them to the center,
			// otherwise find a mineral patch to harvest.
			if (worker->isCarryingGas() || worker->isCarryingMinerals())
			{
				worker->returnCargo();
			}
			else if (!worker->getPowerUp())  // The worker cannot harvest anything if it
			{                             // is carrying a powerup such as a flag
			  // Harvest from the nearest mineral patch or gas refinery
				if (!worker->gather(worker->getClosestUnit(IsMineralField || IsRefinery)))
				{
					// If the call fails, then print the last error message
					Broodwar << Broodwar->getLastError() << std::endl;
				}

			} // closure: has no powerup
		} // closure: if idle

		//////////////////////////////////////////////////////////////////////////////
		// Supply Production
		//////////////////////////////////////////////////////////////////////////////
		UnitType supplyProviderType = worker->getType().getRace().getSupplyProvider();
		static int lastChecked = 0;

		int moreSupply = numProducers / 3;

		if (moreSupply >= 4)
			moreSupply = 4;


		// If we are supply blocked and haven't tried constructing more recently
		if (BWAPI::Broodwar->self()->supplyUsed() + 8 >= BWAPI::Broodwar->self()->supplyTotal() &&
			lastChecked + 400 < Broodwar->getFrameCount() &&
			Broodwar->self()->incompleteUnitCount(supplyProviderType) <= 0 + moreSupply)
		{
			lastChecked = Broodwar->getFrameCount();

			// Retrieve a unit that is capable of constructing the supply needed
			Unit supplyBuilder = worker->getClosestUnit(GetType == supplyProviderType.whatBuilds().first &&
				(IsIdle || IsGatheringMinerals) &&
				IsOwned);
			// If a unit was found
			if (supplyBuilder)
			{
				if (supplyProviderType.isBuilding())
				{
					TilePosition targetBuildLocation = Broodwar->getBuildLocation(supplyProviderType, supplyBuilder->getTilePosition());
					if (targetBuildLocation)
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

							// Order the builder to construct the supply structure
						supplyBuilder->build(supplyProviderType, targetBuildLocation);
					}
				}
				else
				{
					// Train the supply provider (Overlord) if the provider is not a structure
					supplyBuilder->train(supplyProviderType);
				}
			} // closure: supplyBuilder is valid
		} // closure: insufficient supply
	}
}

void WorkerManager::construct(std::list<BWAPI::Unit> _workers, BWAPI::UnitType structure, BWAPI::TilePosition targetLocation)
{
	bool found = false;
	for (auto & worker : _workers)
	{
		if (worker->getType() != BWAPI::UnitTypes::Terran_SCV)
		{
			continue;
		}

		Unit builder = worker->getClosestUnit(GetType == structure.whatBuilds().first &&
			(IsIdle || IsGatheringMinerals) &&
			IsOwned);
		if (builder && !found)
		{
			builder->build(structure, targetLocation);
			found = true;
		}
	}
}

WorkerManager & WorkerManager::Instance()
{
	static WorkerManager instance;
	return instance;
}