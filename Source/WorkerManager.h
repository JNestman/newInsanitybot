#ifndef WORKERMANAGER_H
#define WORKERMANAGER_H



#include "BWEM1.4.1/src/bwem.h"
#include <BWAPI.h>
#include <list>
#include "InformationManager.h"

using namespace BWAPI;
using namespace Filter;

namespace insanitybot
{
	class WorkerManager
	{
		int _lastCheckSupply;
		int _lastCheckBuild;
		std::list<BWAPI::Unit>	_bullyHunters;
		std::list<BWAPI::Unit>	_repairWorkers;
		BWAPI::Unit				_mineralClearer;

	public:

		void initialize();
		void update(InformationManager & _infoManager);
		void construct(std::map<BWAPI::Unit, BWEM::Base *>& _workers, BWAPI::UnitType structure, BWAPI::TilePosition targetLocation);
		void supplyConstruction(std::map<BWAPI::Unit, BWEM::Base *>& _workers, BWAPI::TilePosition targetBuildLocation, int reservedMinerals);
		void assignBullyHunters(std::map<BWAPI::Unit, BWEM::Base *>& _workers, int numberOfEnemies);
		void assignRepairWorkers(std::map<BWAPI::Unit, BWEM::Base *>& _workers, BWAPI::Unit building);
		void assignMineralClearer(std::map<BWAPI::Unit, BWEM::Base *>& _workers, std::vector<BWAPI::Unit> needClearing);

		bool checkSupplyConstruction(int numProducers, int reservedMinerals);
		bool closeEnough(BWAPI::Position location1, BWAPI::Position location2);

		void onUnitDestroy(BWAPI::Unit unit);

		void infoText();
		static WorkerManager & Instance();
	};
}

#endif // !WORKERMANAGER_H