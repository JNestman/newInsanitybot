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

		bool goodToAttack;
		bool haveGathered;

		bool maxSupply;

		BWAPI::Position squadPosition;

		int defenseLastNeededFrame;

	public:
		Squad(BWAPI::Unit unit);

		int infantrySquadSize()						{ return _marines.size() + _medics.size(); };
		int mechSquadSize()							{ return _vultures.size() + _tanks.size() + _goliaths.size(); };
		int specialistSquadSize()					{ return _ghosts.size(); }
		int numMarines()							{ return _marines.size(); };
		int numMedics()								{ return _medics.size(); };
		int numGhosts()								{ return _ghosts.size(); };
		int numVultures()							{ return _vultures.size(); };
		int numTanks()								{ return _tanks.size(); };
		int numGoliaths()							{ return _goliaths.size(); };

		int getDefenseLastNeededFrame()				{ return defenseLastNeededFrame; };
		void setDefenseLastNeededFrame(int frame)	{ defenseLastNeededFrame = frame; };

		BWAPI::Position getSquadPosition();

		bool isGoodToAttack()						{ return goodToAttack; };
		void setGoodToAttack(bool attack)			{ goodToAttack = attack; };
		bool haveGatheredAtForwardPoint()			{ return haveGathered; };
		void setHaveGathered(bool gathered)			{ haveGathered = gathered; };

		void addMarine(BWAPI::Unit unit)			{ _marines.push_back(unit); };
		void addMedic(BWAPI::Unit unit)				{ _medics.push_back(unit); };
		void addVulture(BWAPI::Unit unit)			{ _vultures.push_back(unit); };
		void addTank(BWAPI::Unit unit)				{ _tanks.insert(std::pair<BWAPI::Unit, int>(unit, 0)); };
		void addGoliath(BWAPI::Unit unit)			{ _goliaths.push_back(unit); };

		void attack(BWAPI::Position attackPoint, BWAPI::Position forwardGather, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD);
		void attack(BWAPI::Unit target, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD);
		void gather(BWAPI::Position gatherPoint, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD);

		bool Squad::closeEnough(BWAPI::Position location1, BWAPI::Position location2);

		bool flareTarget(BWAPI::Unit medic, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD);
		bool notInFlareDB(BWAPI::Unit target, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD);
	};
}

#endif // !SQUAD_H