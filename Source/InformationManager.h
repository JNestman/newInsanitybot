#include "BWEM1.4.1/src/bwem.h"
#include "BuildOrder.h"
#include <list>

namespace insanitybot
{
	class InformationManager
	{
		Build												_buildOrder;
		BWAPI::Player										_self;
		BWAPI::Player										_enemy;
		BWEM::Base											*_home;
		std::map<BWAPI::Position, BWEM::Base *>				_otherBases;
		BWAPI::TilePosition									_mainPosition;

		std::list<BWAPI::Unit>								_commandCenters;
		std::list<BWAPI::Unit>								_workers;
		std::list<BWAPI::Unit>								_producers;
		std::list<BWAPI::Unit>								_combatUnits;
		std::list<BWAPI::Unit>								_comSats;
		std::list<BWAPI::Unit>								_addons;

		int													_reservedMinerals;
		int													_reservedGas;

	public:
		InformationManager();
		~InformationManager() {};
		void initialize();
		void update();
		bool closeEnough(BWAPI::Position location1, BWAPI::Position location2);

		//Getters
		std::list<BWAPI::Unit> getCommandCenters()	{ return _commandCenters; };
		std::list<BWAPI::Unit> getWorkers()			{ return _workers; };
		std::list<BWAPI::Unit> getProducers()		{ return _producers; };
		std::list<BWAPI::Unit> getCombatUnits()		{ return _combatUnits; };
		std::list<BWAPI::Unit> getComsats()			{ return _comSats; };
		std::list<BWAPI::Unit> getAddons()			{ return _addons; };
		int getNumWorkersOwned()					{ return _workers.size(); };
		int getNumProducers()						{ return _producers.size() + _commandCenters.size(); };
		int getNumFinishedUnit(BWAPI::UnitType type);
		int getNumUnfinishedUnit(BWAPI::UnitType type);
		int getNumTotalUnit(BWAPI::UnitType type);

		int getReservedMinerals()					{ return _reservedMinerals; }
		int getReservedGas()						{ return _reservedGas; }

		BWAPI::TilePosition getMainPosition()		{ return _mainPosition; }

		//Setters
		void setReservedMinerals(int _reserve)		{ _reservedMinerals = _reserve; }
		void setReservedGas(int _reserve)			{ _reservedGas = _reserve; }

		int numWorkersWanted();
		std::list<BWAPI::UnitType>& getQueue() { return _buildOrder._queue; };


		static InformationManager & Instance();
	};
}

