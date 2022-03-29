#ifndef CREATIONMANAGER_H
#define CREATIONMANAGER_H


#include "BWEM1.4.1/src/bwem.h"
#include <BWAPI.h>
#include "WorkerManager.h"
#include "BuildingPlacer.h"
#include "InformationManager.h"

namespace insanitybot
{
	class CreationManager
	{
		BWAPI::Player										_self;
		BuildingPlacer										_buildingPlacer;

		std::map<BWAPI::UnitType, int>						_constructionQueue;
		
	public:
		CreationManager();
		~CreationManager() {};

		void initialize();

		void update(InformationManager & _infoManager);
		void checkQueue(BWAPI::Unit unit, std::list<BWAPI::UnitType>& queue, int &reservedMinerals, int &reservedGas);

		static CreationManager & Instance();
	};
}

#endif