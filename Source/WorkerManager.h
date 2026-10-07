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
		struct PendingConstruction
		{
			BWAPI::UnitType structure;
			BWAPI::TilePosition targetLocation;
			BWEM::Base *base;
			int lastBuildAttempt;
		};

		int _lastCheckSupply;
		int _lastCheckBuild;
		BWAPI::Unit				_mineralClearer;
		std::map<BWAPI::Unit, PendingConstruction> _pendingConstructions;

		bool gasCutOff;
		void updatePendingConstructions();

	public:

		void initialize();
		void update(InformationManager & _infoManager);
		void construct(std::map<BWAPI::Unit, BWEM::Base *>& _workers, BWAPI::UnitType structure, BWAPI::TilePosition targetLocation, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases, InformationManager & _infoManager);
		void supplyConstruction(std::map<BWAPI::Unit, BWEM::Base *>& _workers, BWAPI::TilePosition targetBuildLocation, int reservedMinerals, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases, InformationManager & _infoManager);
		void assignBullyHunters(std::map<BWAPI::Unit, BWEM::Base *>& _workers, std::list<BWAPI::Unit>& _bullyHunters, int numberOfEnemies, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases);
		void assignRepairWorkers(std::map<BWAPI::Unit, BWEM::Base *>& _workers, std::list<BWAPI::Unit>& _repairWorkers, BWAPI::Unit building, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases);
		void assignMineralClearer(std::map<BWAPI::Unit, BWEM::Base *>& _workers, std::vector<BWAPI::Unit> needClearing, std::map<BWAPI::Position, BWEM::Base *> & _ownedBases);
		void handleIslandConstruction(std::map<BWAPI::Unit, BWEM::Base *>& _islandWorkers, std::map<BWAPI::Position, BWEM::Base *> & _ownedIslandBases, std::list<BWAPI::Unit> _engibays, BWAPI::TilePosition targetLocation);

		bool checkSupplyConstruction(int numProducers, int reservedMinerals);
		bool closeEnough(BWAPI::Position location1, BWAPI::Position location2);
		bool hasPendingConstruction(BWAPI::UnitType structure) const
		{
			for (const auto & construction : _pendingConstructions)
				if (construction.second.structure == structure)
					return true;
			return false;
		}
		bool hasPendingBuildOrderConstruction() const
		{
			for (const auto & construction : _pendingConstructions)
				if (construction.second.structure != BWAPI::UnitTypes::Terran_Supply_Depot)
					return true;
			return false;
		}

		int getLastCheckSupply() { return _lastCheckSupply; };
		void setLastCheckSupply(int lastCheckSupply) { _lastCheckSupply = lastCheckSupply; };

		static WorkerManager & Instance();
	};
}

#endif // !WORKERMANAGER_H