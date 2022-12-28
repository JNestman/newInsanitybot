#ifndef WORKERMANAGER_H
#define WORKERMANAGER_H



#include "BWEM1.4.1/src/bwem.h"
#include <BWAPI.h>
#include "InformationManager.h"

using namespace BWAPI;
using namespace Filter;

namespace insanitybot
{
	class WorkerManager
	{
		int _lastCheckSupply;
		int _lastCheckBuild;
		BWAPI::Unit				_mineralClearer;

		bool gasCutOff;

	public:

		void initialize();
		void update(InformationManager & _infoManager);
		void construct(std::map<BWAPI::Unit, BWEM::Base *>& _workers, BWAPI::UnitType structure, BWAPI::TilePosition targetLocation, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases);
		void supplyConstruction(std::map<BWAPI::Unit, BWEM::Base *>& _workers, BWAPI::TilePosition targetBuildLocation, int reservedMinerals, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases);
		void assignBullyHunters(std::map<BWAPI::Unit, BWEM::Base *>& _workers, std::list<BWAPI::Unit>& _bullyHunters, int numberOfEnemies, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases);
		void assignRepairWorkers(std::map<BWAPI::Unit, BWEM::Base *>& _workers, std::list<BWAPI::Unit>& _repairWorkers, BWAPI::Unit building, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases);
		void assignMineralClearer(std::map<BWAPI::Unit, BWEM::Base *>& _workers, std::vector<BWAPI::Unit> needClearing, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases);
		void handleIslandConstruction(std::map<BWAPI::Unit, BWEM::Base *>& _islandWorkers, std::map<BWAPI::Position, BWEM::Base *> & _ownedIslandBases, std::list<BWAPI::Unit> _engibays, BWAPI::TilePosition targetLocation);

		bool checkSupplyConstruction(int numProducers, int reservedMinerals);
		bool closeEnough(BWAPI::Position location1, BWAPI::Position location2);

		static WorkerManager & Instance();
	};
}

#endif // !WORKERMANAGER_H