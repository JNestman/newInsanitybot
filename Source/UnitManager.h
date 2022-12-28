#ifndef UNITMANAGER_H
#define UNITMANAGER_H

#include "BWEM1.4.1/src/bwem.h"
#include <BWAPI.h>
#include "InformationManager.h"
#include "Squad.h"

namespace insanitybot
{
	class UnitManager
	{
		std::list<Squad> _infantrySquads;
		std::list<Squad> _mechSquads;
		std::list<Squad> _allInSquad;
		std::list<Squad> _defensiveSquads; 
		std::list<Squad> _frontierSquads;
		std::list<Squad> _dropSquad;
		std::list<Squad> _specialistSquad;
		
		int infantrySquadSizeLimit;
		int mechSquadSizeLimit;
		int specialistSquadSizeLimit;
		int frontierSquadSizeLimit;
		bool needDefense;

		int numEmptyBases;

		//            Vessel,                target, frame targeted
		std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>> _irradiateDB;
		//            medic,                target, frame targeted
		std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>> _flareBD;

	public:
		UnitManager();

		void update(InformationManager & _infoManager);
		BWAPI::Position getForwardPoint(InformationManager & _infoManager);
		BWAPI::Position getGhostForwardPoint(InformationManager & _infoManager);
		void handleScout(BWAPI::Unit & _scout);
		bool assignSquad(BWAPI::Unit unassigned, bool bio, bool allIn);

		void assignBio(InformationManager & _infoManager);
		void assignMech(InformationManager & _infoManager);
		void assignAllIn(InformationManager & _infoManager);

		void assignFieldEngineers(InformationManager & _infoManager);
		void checkDeadEngineers(InformationManager & _infoManager);
		void handleFieldEngineers(InformationManager & _infoManager, BWAPI::TilePosition nextUp);

		void handleFloaters(InformationManager & _infoManager, BWAPI::TilePosition nextUp);

		bool irradiateTarget(BWAPI::Unit vessel);
		bool notIrradiateTarged(BWAPI::Unit potentialTarget);


		void checkFlareDBForTimeout();

		void infoText();
	};
}

#endif // !UNITMANAGER_H