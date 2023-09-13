#ifndef UNITMANAGER_H
#define UNITMANAGER_H

#include "BWEM1.4.1/src/bwem.h"
#include <BWAPI.h>
#include "InformationManager.h"
#include "Squad.h"

// Used to bring flying units around the edge of the map
const int WaypointSpacing = 5;

namespace insanitybot
{
	class UnitManager
	{
		std::list<Squad> _infantrySquads;
		std::list<Squad> _mechSquads;
		std::list<Squad> _allInSquad;
		std::list<Squad> _defensiveSquads; 
		std::list<Squad> _frontierSquads;
		std::list<Squad> _specialistSquad;
		std::list<Squad> _dropSquad;
		std::list<Squad> _BCsquad;

		std::vector<BWAPI::Position>    _waypoints;
		
		int infantrySquadSizeLimit;
		int mechSquadSizeLimit;
		int specialistSquadSizeLimit;
		int frontierSquadSizeLimit;
		int dropSquadSizeLimit;
		int numEmptyBases;

		int _direction;
		int	_nextWaypointIndex;
		int	_lastWaypointIndex;

		bool needDefense;
		bool loadingDrop;
		bool dropping;

		BWAPI::TilePosition scoutNextTarget;
		std::list<BWAPI::Position> circlePositions;

		//            Vessel,                target, frame targeted
		std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>> _irradiateDB;
		std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>> _MatrixDB;
		//            medic,                target, frame targeted
		std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>> _flareBD;

	public:
		UnitManager();

		void update(InformationManager & _infoManager);
		BWAPI::Position getForwardPoint(InformationManager & _infoManager);
		BWAPI::Position getGhostForwardPoint(InformationManager & _infoManager);
		void handleScout(BWAPI::Unit & _scout, const BWAPI::Position enemyMain);
		bool assignSquad(BWAPI::Unit unassigned, bool bio, bool allIn);
		bool assignAirSquad(BWAPI::Unit unassigned);

		void assignBio(InformationManager & _infoManager);
		void assignMech(InformationManager & _infoManager);
		void assignAllIn(InformationManager & _infoManager);
		void assignAir(InformationManager & _infoManager);

		void assignFieldEngineers(InformationManager & _infoManager);
		void checkDeadEngineers(InformationManager & _infoManager);
		void handleFieldEngineers(InformationManager & _infoManager, BWAPI::TilePosition nextUp);

		void handleFloaters(InformationManager & _infoManager, BWAPI::TilePosition nextUp);

		void handleDropships(InformationManager & _infoManager);
		bool haveOpenDropMarines();
		bool haveOpenDropMedics();
		BWAPI::Position TileCenter(const BWAPI::TilePosition & tile);
		int waypointIndex(int i);
		const BWAPI::Position & waypoint(int i);
		void calculateWaypoints();
		void followPerimiter(BWAPI::Unit airship, BWAPI::Position target);

		bool irradiateTarget(BWAPI::Unit vessel);
		bool notInIrradiateDB(BWAPI::Unit potentialTarget);

		bool matrixTarget(BWAPI::Unit vessel);
		bool notInMatrixDB(BWAPI::Unit potentialTarget);

		void checkFlareDBForTimeout();

		void infoText();
	};
}

#endif // !UNITMANAGER_H