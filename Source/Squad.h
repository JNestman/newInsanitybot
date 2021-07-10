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
		int numMarines()							{ return _marines.size(); };
		int numMedics()								{ return _medics.size(); };
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

		bool removeMarine(BWAPI::Unit unit);
		bool removeMedic(BWAPI::Unit unit);
		bool removeVulture(BWAPI::Unit unit);
		bool removeTank(BWAPI::Unit unit);
		bool removeGoliath(BWAPI::Unit unit);

		std::list<BWAPI::Unit>& getMedics()			{ return _medics; }

		void attack(BWAPI::Position attackPoint, BWAPI::Position forwardGather);
		void attack(BWAPI::Unit target);
		void gather(BWAPI::Position gatherPoint);

		bool Squad::closeEnough(BWAPI::Position location1, BWAPI::Position location2);
	};
}

#endif // !SQUAD_H