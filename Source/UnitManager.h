#ifndef UNITMANAGER_H
#define UNITMANAGER_H

#include "BWEM1.4.1/src/bwem.h"
#include <BWAPI.h>
#include <list>
#include "InformationManager.h"
#include "Squad.h"

namespace insanitybot
{
	class UnitManager
	{
		std::list<Squad> _infantrySquads;
		std::list<Squad> _mechSquads;
		std::list<Squad> _defensiveSquads;
		int infantrySquadSizeLimit;
		int mechSquadSizeLimit;
		bool needDefense;

		//            Vessel,                target, frame targeted
		std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>> _irradiateDB;

	public:
		UnitManager();

		void onUnitDestroy(BWAPI::Unit unit);

		void update(InformationManager & _infoManager);
		void handleScout(BWAPI::Unit & _scout);
		void assignSquad(BWAPI::Unit unassigned, bool bio);

		bool irradiateTarget(BWAPI::Unit vessel);
		bool notIrradiateTarged(BWAPI::Unit potentialTarget);

		void infoText();
	};
}

#endif // !UNITMANAGER_H