#ifndef SQUAD_H
#define SQUAD_H

#include "BWEM1.4.1/src/bwem.h"
#include <BWAPI.h>

namespace insanitybot
{
	class Squad
	{
		std::list <BWAPI::Unit> _marines;
		std::list <BWAPI::Unit> _medics;
		std::list <BWAPI::Unit> _ghosts;

		std::list <BWAPI::Unit> _vultures;
		std::map <BWAPI::Unit, int> _tanks;
		std::list <BWAPI::Unit> _goliaths;

		BWAPI::Unit nuker;

		bool goodToAttack;
		bool haveGathered;

		int maxSupply;
		bool selfPreservationToTheWind;

		BWAPI::Position squadPosition;

		BWAPI::TilePosition frontierLocation;

		int defenseLastNeededFrame;

	public:
		Squad(BWAPI::Unit unit, bool isAllIn);

		int infantrySquadSize()						{ return _marines.size() + _medics.size(); };
		int mechSquadSize()							{ return _vultures.size() + _tanks.size() + _goliaths.size(); };
		int specialistSquadSize()					{ return _ghosts.size(); }
		int frontierSquadSize()						{ return _marines.size() + _medics.size() + _vultures.size() + _tanks.size() + _goliaths.size(); };
		int numMarines()							{ return _marines.size(); };
		int numMedics()								{ return _medics.size(); };
		int numGhosts()								{ return _ghosts.size(); };
		int numVultures()							{ return _vultures.size(); };
		int numTanks()								{ return _tanks.size(); };
		int numGoliaths()							{ return _goliaths.size(); };

		std::map <BWAPI::Unit, int> getTanks()		{ return _tanks; };

		int getDefenseLastNeededFrame()				{ return defenseLastNeededFrame; };
		void setDefenseLastNeededFrame(int frame)	{ defenseLastNeededFrame = frame; };

		BWAPI::Position getSquadPosition();

		BWAPI::TilePosition getFrontierLocation()				{ return frontierLocation; };
		void setFrontierLocation(BWAPI::TilePosition target)	{ frontierLocation = target; };

		bool isGoodToAttack()						{ return goodToAttack; };
		void setGoodToAttack(bool attack)			{ goodToAttack = attack; };
		bool haveGatheredAtForwardPoint()			{ return haveGathered; };
		void setHaveGathered(bool gathered)			{ haveGathered = gathered; };

		BWAPI::Unit getNuker()						{ return nuker; };
		void clearNuker()							{ nuker = NULL; };

		bool isMaxSupply()							{ return selfPreservationToTheWind; };

		void addMarine(BWAPI::Unit unit)			{ _marines.push_back(unit); };
		void addMedic(BWAPI::Unit unit)				{ _medics.push_back(unit); };
		void addGhost(BWAPI::Unit unit)				{ _ghosts.push_back(unit); };
		void addVulture(BWAPI::Unit unit)			{ _vultures.push_back(unit); };
		void addTank(BWAPI::Unit unit)				{ _tanks.insert(std::pair<BWAPI::Unit, int>(unit, 0)); };
		void addGoliath(BWAPI::Unit unit)			{ _goliaths.push_back(unit); };

		void attack(BWAPI::Position attackPoint, BWAPI::Position forwardGather, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD);
		void attack(BWAPI::Unit target, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD);
		void gather(BWAPI::Position gatherPoint, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD);

		void protect();

		void handleNuker(BWAPI::Position target);
		void setNuker();

		bool tooSpreadOut();

		void groundKiteMicro(BWAPI::Unit & friendly, BWAPI::Position enemy);

		BWAPI::Position stormDodge(BWAPI::Position friendly, BWAPI::Position enemy);

		bool Squad::closeEnough(BWAPI::Position location1, BWAPI::Position location2);

		bool flareTarget(BWAPI::Unit medic, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD);
		bool notInFlareDB(BWAPI::Unit target, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD);
	};
}

#endif // !SQUAD_H