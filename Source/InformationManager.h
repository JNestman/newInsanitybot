#ifndef INFOMATIONMANAGER_H
#define INFOMATIONMANAGER_H

#include "BWEM1.4.1/src/bwem.h"

namespace insanitybot
{
	class InformationManager
	{
		// Deletion functions
		void checkForDeadList(std::list<BWAPI::Unit>& listToDeleteFrom);
		void checkForDeadMap(std::map<BWAPI::Unit, int>& mapToDeleteFrom);
		void checkForDeadCenters();
		void checkForDeadRefineries();

		// Variables
		std::string											_strategy;
		std::vector<std::string>							_1BaseStrat {"8RaxDef", "8RaxAgg", "2Fac", "TONK"};
		std::vector<std::string>							_2BaseStrat	{"SKTerran", "Mech", "Nuke"};

		std::list<BWAPI::UnitType>							_queue;
		BWAPI::Player										_self;
		BWAPI::Player										_enemy;
		BWAPI::Race											_enemyRace;
		BWEM::Base											*_home;
		BWEM::Base											*_natural;
		BWAPI::Position										_mainChoke;
		BWAPI::Position										_naturalChoke;
		BWAPI::TilePosition									_enemyMainPos;
		BWAPI::Position										_enemyNatPos;
		std::map<BWAPI::Position, BWEM::Base *>				_ownedBases;
		std::map<BWAPI::Position, BWEM::Base *>				_otherBases;
		std::vector<BWAPI::TilePosition>					_scanRotation;
		std::vector<BWAPI::TilePosition>					_squadScoutRotation;
		std::map<BWAPI::Position, BWEM::Base *>				_islandBases;
		std::map<BWAPI::Position, BWEM::Base *>				_ownedIslandBases;

		std::map<BWAPI::Position, BWEM::Base *>				_enemyBases;
		std::list<BWAPI::Position>							_enemyStructurePositions;
		BWAPI::Unitset										_enemyUnits;
		bool												_enemyPool;

		std::list<BWAPI::Unit>								_neutralBuildings;

		BWAPI::TilePosition									_mainPosition;

		BWAPI::Unitset										_smallMinerals;
		BWAPI::Unitset										_islandSmallMinerals;

		std::list<BWAPI::Unit>								_commandCenters;
		std::list<BWAPI::Unit>								_islandCenters;
		std::map<BWAPI::Unit, BWEM::Base *>					_workers;
		std::list<BWAPI::Unit>								_bullyHunters;
		std::list<BWAPI::Unit>								_repairWorkers;
		std::map<BWAPI::Unit, BWEM::Base *>					_islandWorkers;

		std::list<BWAPI::Unit>								_refineries;

		BWAPI::Unit											_islandBuilder;

		std::list<BWAPI::Unit>								_barracks;
		std::list<BWAPI::Unit>								_factories;
		std::list<BWAPI::Unit>								_starports;

		std::list<BWAPI::Unit>								_academy;
		std::list<BWAPI::Unit>								_engibays;
		std::list<BWAPI::Unit>								_armories;
		std::list<BWAPI::Unit>								_science;

		std::map<BWAPI::Unit, int>							_marines;
		std::map<BWAPI::Unit, int>							_medics;
		std::map<BWAPI::Unit, int>							_dropships;
		std::map<BWAPI::Unit, int>							_vessels;
		std::map<BWAPI::Unit, int>							_vultures;
		std::map<BWAPI::Unit, int>							_tanks;
		std::map<BWAPI::Unit, int>							_goliaths;

		std::list<BWAPI::Unit>								_comsats;
		int													_lastScanFrame;

		std::list<BWAPI::Unit>								_addons;
		std::list<BWAPI::Unit>								_machineShops;
		std::list<BWAPI::Unit>								_construction;
		std::list<BWAPI::Unit>								_injuredBuildings;
		std::list<BWAPI::Unit>								_bunkers;
		BWAPI::TilePosition									_mainBunkerPos;
		BWAPI::TilePosition									_NatBunkerPos;

		std::list<BWAPI::Unit>								_turrets;

		BWAPI::Unit											_scout;
		std::list<BWAPI::Unit>								_floatingBuildings;

		int													_reservedMinerals;
		int													_reservedGas;

		bool												_initialBarracks;
		bool												_attack;
		bool												_islandExpand;

		bool												_enemyHasAir;
		bool												_enemyRushing;

	public:
		InformationManager();
		~InformationManager() {};
		void initialize();
		void checkQueue(BWAPI::Unit myUnit);
		void update();
		
		// Build Orders here
		void updateBuildOrder();
		void SKTerran();
		void Mech();
		void EightRaxDef();

		bool closeEnough(BWAPI::Position location1, BWAPI::Position location2);
		void onUnitShow(BWAPI::Unit unit);
		void onUnitDestroy(BWAPI::Unit unit);
		void onUnitRenegade(BWAPI::Unit unit);
		void onUnitComplete(BWAPI::Unit unit);

		bool shouldExpand();
		bool areExpanding();
		bool shouldHaveDefenseSquad(bool worker);

		bool checkForEnemyRush();
		
		std::vector<BWAPI::Unit> mineralsNeedClear();

		//Getters
		std::string getStrategy()										{ return _strategy; };

		BWAPI::Race getEnemyRace()										{ return _enemyRace; };

		std::list<BWAPI::Unit> getCommandCenters()						{ return _commandCenters; };
		std::list<BWAPI::Unit> getRefineries()							{ return _refineries; }
		std::list<BWAPI::Unit> getIslandCenters()						{ return _islandCenters; };
		std::map<BWAPI::Unit, BWEM::Base *>& getWorkers()				{ return _workers; };
		std::list<BWAPI::Unit>& getBullyHunters()						{ return _bullyHunters; };
		std::list<BWAPI::Unit>& getRepairWorkers()						{ return _repairWorkers; };
		std::map<BWAPI::Unit, BWEM::Base *>& getIslandWorkers()			{ return _islandWorkers; };
		std::list<BWAPI::Unit> getBarracks()							{ return _barracks; };
		std::list<BWAPI::Unit> getFactories()							{ return _factories; };
		std::list<BWAPI::Unit> getStarports()							{ return _starports; };

		std::list<BWAPI::Unit> getAcademy()								{ return _academy; };
		std::list<BWAPI::Unit> getEngibays()							{ return _engibays; };
		std::list<BWAPI::Unit> getArmories()							{ return _armories; };
		std::list<BWAPI::Unit> getScience()								{ return _science; };

		BWAPI::Unit & getIslandBuilder()								{ return _islandBuilder; };
		BWAPI::Unitset getIslandSmallMinerals()							{ return _islandSmallMinerals; };

		std::map<BWAPI::Unit, int>& getMarines()						{ return _marines; };
		std::map<BWAPI::Unit, int>& getMedics()							{ return _medics; };
		std::map<BWAPI::Unit, int>& getDropships()						{ return _dropships; };
		std::map<BWAPI::Unit, int>& getVessels()						{ return _vessels; };
		std::map<BWAPI::Unit, int>& getVultures()						{ return _vultures; };
		std::map<BWAPI::Unit, int>& getTanks()							{ return _tanks; };
		std::map<BWAPI::Unit, int>& getGoliaths()						{ return _goliaths; };

		std::list<BWAPI::Unit> getComsats()								{ return _comsats; };
		std::list<BWAPI::Unit> getAddons()								{ return _addons; };
		std::list<BWAPI::Unit> getMachineShops()						{ return _machineShops; };
		std::list<BWAPI::Unit> getConstructing()						{ return _construction; };
		std::list<BWAPI::Unit> getInjuredBuildings()					{ return _injuredBuildings; };
		std::list<BWAPI::Unit> getTurrets()								{ return _turrets; };
		std::list<BWAPI::Unit> getBunkers()								{ return _bunkers; };
		BWAPI::TilePosition getMainBunkerPos()							{ return _mainBunkerPos; };
		BWAPI::TilePosition getNatBunkerPos()							{ return _NatBunkerPos; };

		std::map<BWAPI::Position, BWEM::Base *>& getOwnedBases()		{ return _ownedBases; };
		std::map<BWAPI::Position, BWEM::Base *>& getOtherBases()		{ return _otherBases; };
		std::map<BWAPI::Position, BWEM::Base *>& getIslandBases()		{ return _islandBases; };
		std::map<BWAPI::Position, BWEM::Base *>& getOwnedIslandBases()	{ return _ownedIslandBases; };
		std::map<BWAPI::Position, BWEM::Base *>& getEnemyBases()		{ return _enemyBases; };
		std::list<BWAPI::Position>& getEnemyBuildingPositions()			{ return _enemyStructurePositions; };
		std::vector<BWAPI::TilePosition>& getSquadScoutLocations()		{ return _squadScoutRotation; };

		std::list<BWAPI::Unit>& getNeutralBuildings()					{ return _neutralBuildings; };

		BWAPI::Unit & getScout()										{ return _scout; };
		std::list<BWAPI::Unit>& getFloatingBuildings()					{ return _floatingBuildings; };

		BWAPI::Position getMainChokePos()								{ return _mainChoke; };
		BWAPI::Position getNaturalChokePos()							{ return _naturalChoke; };
		BWAPI::Position getEnemyNaturalPos()							{ return _enemyNatPos; };
		int getNumWorkersOwned()										{ return _workers.size() + _islandWorkers.size() + _bullyHunters.size() + _repairWorkers.size(); };
		int getNumProducers()											{ return _barracks.size() + _factories.size() + _starports.size() + _commandCenters.size(); };
		int getNumFinishedUnit(BWAPI::UnitType type);
		int getNumUnfinishedUnit(BWAPI::UnitType type);
		int getNumTotalUnit(BWAPI::UnitType type);
		int numGeyserBases();

		int getReservedMinerals()					{ return _reservedMinerals; }
		int getReservedGas()						{ return _reservedGas; }

		int & getAndAlterMinerals()					{ return _reservedMinerals;  }
		int & getAndAlterGas()						{ return _reservedGas; }

		BWAPI::TilePosition getMainPosition()		{ return BWAPI::TilePosition(_home->Center()); }
		BWAPI::TilePosition getNatPosition()		{ return BWAPI::TilePosition(_natural->Center()); }

		bool getAggression()						{ return _attack; };
		bool isOneBasePlay(std::string strat)		{ return std::find(_1BaseStrat.begin(), _1BaseStrat.end(), strat) != _1BaseStrat.end(); };
		bool isTwoBasePlay(std::string strat)		{ return std::find(_2BaseStrat.begin(), _2BaseStrat.end(), strat) != _2BaseStrat.end(); };

		bool shouldIslandExpand()					{ return _islandExpand; };

		bool enemyHasAir()							{ return _enemyHasAir; };

		bool armoryDone()							{ return getNumFinishedUnit(BWAPI::UnitTypes::Terran_Armory); };

		//Setters
		std::string setStrategy(std::string strat)	{ _strategy = strat; }

		void setReservedMinerals(int _reserve)		{ _reservedMinerals = _reserve; }
		void setReservedGas(int _reserve)			{ _reservedGas = _reserve; }
		void setMainPosition(BWAPI::TilePosition _mainPos) { _mainPosition = _mainPos; }
		void setAggression(bool aggression)			{ _attack = aggression; };

		void setIslandExpand(bool expand)			{ _islandExpand = expand; };
		void setIslandBuilder(BWAPI::Unit unit)		{ _islandBuilder = unit; };

		int numWorkersWanted();
		std::list<BWAPI::UnitType>& getQueue() { return _queue; };
		int queueSize() { return _queue.size(); };


		static InformationManager & Instance();
	};
}

#endif