#include "InformationManager.h"
#include <time.h>
#include "BuildOrder.h"
#include "ReadWrite.h"

namespace { auto & theMap = BWEM::Map::Instance(); }

using namespace insanitybot;

// ============================================================
// Constructor / Instance
// ============================================================

insanitybot::InformationManager::InformationManager()
	: _self(BWAPI::Broodwar->self())
	, _enemy(BWAPI::Broodwar->enemy())
	, _buildOrder(new BuildOrder())
	, _readWrite(new ReadWrite())
{
	_attack = false;
	_islandExpand = false;
	_lastScanFrame = 0;
	_enemyHasAir = false;
	_enemyRushing = false;
	_enemyHasDtLurker = false;
	_enemyPool = false;
	_enemyCyberCore = false;
	_enemyHydraDen = false;
	_enemyLair = false;
	_enemyWorkerNumber = 0;
	_targetDefended = false;
	_nukeDotDetected = false;
	_waitASec = 0;
	_pauseGas = false;

	for (auto & u : BWAPI::Broodwar->self()->getUnits())
	{
		if (!u || !u->exists()) continue;
		if (u->getType().isResourceDepot())
		{
			_mainNotTilePos = u->getPosition();
			setMainPosition(u->getTilePosition());
			break;
		}
	}

	_readWrite->initialize();
	_strategy = _readWrite->getChosenBuildOrder(_mainNotTilePos);
	_buildOrder->initialize(_strategy);
	_ourInitialStrategy = _strategy;

	initBuildOrderDispatch();
}

InformationManager & InformationManager::Instance()
{
	static InformationManager instance;
	return instance;
}

// ============================================================
// Build order dispatch table
// Adding a new strategy: add one entry here, nothing else.
// ============================================================

void InformationManager::initBuildOrderDispatch()
{
	_buildOrderDispatch["Mech"] = [](BuildOrder & bo, InformationManager & im) { bo.Mech(im); };
	_buildOrderDispatch["GreedMech"] = [](BuildOrder & bo, InformationManager & im) { bo.GreedMech(im); };
	_buildOrderDispatch["MechVT"] = [](BuildOrder & bo, InformationManager & im) { bo.MechVT(im); };
	_buildOrderDispatch["FiveFacGol"] = [](BuildOrder & bo, InformationManager & im) { bo.FiveFacGol(im); };
	_buildOrderDispatch["Nuke"] = [](BuildOrder & bo, InformationManager & im) { bo.Nuke(im); };
	_buildOrderDispatch["BioDrops"] = [](BuildOrder & bo, InformationManager & im) { bo.BioDrops(im); };
	_buildOrderDispatch["BCMeme"] = [](BuildOrder & bo, InformationManager & im) { bo.BCMeme(im); };
	_buildOrderDispatch["8RaxDef"] = [](BuildOrder & bo, InformationManager & im) { bo.EightRaxDef(im); };
	_buildOrderDispatch["1BaseMech"] = [](BuildOrder & bo, InformationManager & im) { bo.OneBaseMech(im); };
	_buildOrderDispatch["OneFacAllIn"] = [](BuildOrder & bo, InformationManager & im) { bo.OneFacAllIn(im); };
	_buildOrderDispatch["MechAllIn"] = [](BuildOrder & bo, InformationManager & im) { bo.MechAllIn(im); };
	_buildOrderDispatch["SKTerran"] = [](BuildOrder & bo, InformationManager & im) { bo.SKTerran(im); };
}

// ============================================================
// initialize - runs once at game start
// ============================================================

void InformationManager::initialize()
{
	_scout = NULL;
	_islandBuilder = NULL;

	_enemyNatPos = BWAPI::Position(0, 0);
	_enemyMainPos = BWAPI::TilePosition(0, 0);
	_enemyNatChoke = BWAPI::Position(0, 0);

	_queue.clear();
	_initialBarracks = false;
	_reservedMinerals = 0;
	_reservedGas = 0;

	BWAPI::Position main;
	for (auto & u : BWAPI::Broodwar->self()->getUnits())
	{
		if (!u || !u->exists()) continue;
		if (u->getType().isResourceDepot()) { main = u->getPosition(); break; }
	}

	// Classify all map bases into owned / other / island
	for (const BWEM::Area & area : theMap.Areas())
	{
		for (const BWEM::Base & base : area.Bases())
		{
			std::vector<BWEM::Ressource *> res;
			for (BWEM::Mineral * m : base.Minerals()) res.push_back(m);
			for (BWEM::Geyser * g : base.Geysers())  res.push_back(g);

			BWEM::Area areaCopy = area;

			if (closeEnough(BWAPI::Position(base.Location()), main))
			{
				_home = new BWEM::Base(&areaCopy, base.Location(), res, base.BlockingMinerals());
				_ownedBases[BWAPI::Position(base.Location())] =
					new BWEM::Base(&areaCopy, base.Location(), res, base.BlockingMinerals());
			}
			else
			{
				int length = -1;
				theMap.GetPath(BWAPI::Position(base.Location()), BWAPI::Position(_mainPosition), &length);

				auto * newBase = new BWEM::Base(&areaCopy, base.Location(), res, base.BlockingMinerals());
				if (length < 0)
					_islandBases[BWAPI::Position(base.Location())] = newBase;
				else
					_otherBases[BWAPI::Position(base.Location())] = newBase;
			}
		}
	}

	// Seed initial workers and command center
	for (auto & u : BWAPI::Broodwar->self()->getUnits())
	{
		if (!u || !u->exists()) continue;
		if (u->getType().isWorker())
			_workers.insert(std::make_pair(u, _ownedBases.begin()->second));
		else if (u->getType().isResourceDepot())
			_commandCenters.push_back(u);
	}

	// Find natural expansion (closest geyser base to main by path length)
	int shortest = 999999;
	int length = -1;
	BWEM::CPPath mainToNat;

	for (auto & base : _otherBases)
	{
		if (!base.second->baseHasGeyser()) continue;
		BWEM::CPPath temp = theMap.GetPath(BWAPI::Position(base.first), BWAPI::Position(_mainPosition), &length);
		if (length < 0) continue;
		if (length < shortest) { mainToNat = temp; shortest = length; _natural = base.second; }
	}

	// Natural choke
	shortest = 999999;
	int distance = 0;
	length = -1;

	for (auto & base : _otherBases)
	{
		if (base.second == _natural) continue;
		BWEM::CPPath path = theMap.GetPath(BWAPI::Position(base.first), BWAPI::Position(_natural->Center()), &length);
		if (length < 0) continue;
		for (auto choke : path)
		{
			distance = abs((BWAPI::Position(choke->Center()).x - _natural->Center().x)
				- (BWAPI::Position(choke->Center()).y - _natural->Center().y));
			if (distance < shortest) { _naturalChoke = BWAPI::Position(choke->Center()); shortest = distance; }
		}
		break;
	}

	// Per-map natural choke adjustments
	const std::string mapHash = BWAPI::Broodwar->mapHash();
	const int mapW = BWAPI::Broodwar->mapWidth();
	const int mapH = BWAPI::Broodwar->mapHeight();
	const BWAPI::Position natLoc = BWAPI::Position(_natural->Location());

	if (mapHash == "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b") // Destination
	{
		_naturalChoke += (_mainPosition.y > mapH / 2) ? BWAPI::Position(-150, 200) : BWAPI::Position(-100, -150);
	}
	else if (mapHash == "c8386b87051f6773f6b2681b0e8318244aa086a6") // Neo Moon Glaive
	{
		if (_mainPosition.y < mapH / 2) _naturalChoke += BWAPI::Position(300, 0);
	}
	else if (mapHash == "d2f5633cc4bb0fca13cd1250729d5530c82c7451") // Fighting Spirit
	{
		if (_mainPosition.y < mapH / 2 && _mainPosition.x > mapW / 2) // Top Right
			_naturalChoke += BWAPI::Position(50, -200);
	}
	else if (mapHash == "9a4498a896b28d115129624f1c05322f48188fe0") // Road Runner
	{
		if (_mainPosition.y > mapH * .80) _naturalChoke = BWAPI::Position(natLoc.x, natLoc.y - 150);
		else if (_mainPosition.y < mapH * .20) _naturalChoke = BWAPI::Position(natLoc.x + 100, natLoc.y + 200);
	}
	else if (mapHash == "de2ada75fbc741cfa261ee467bf6416b10f9e301") // Python
	{
		if (_mainPosition.y < mapH * .30 || _mainPosition.x < mapW * .30)
			_naturalChoke = BWAPI::Position(natLoc.x + 300, natLoc.y + 200);
	}
	else if (mapHash == "9bfc271360fa5bab3707a29e1326b84d0ff58911") // Tao Cross
	{
		if (_mainPosition.x < mapW * .30) _naturalChoke = BWAPI::Position(natLoc.x + 300, natLoc.y + 100);
	}
	else if (mapHash == "1e983eb6bcfa02ef7d75bd572cb59ad3aab49285") // Andromeda
	{
		if (_mainPosition.y > mapH * .80 && _mainPosition.x > mapW * .30)
			_naturalChoke = BWAPI::Position(natLoc.x - 50, natLoc.y - 150);
	}
	else if (mapHash == "dbd844012e678b23ca8ef21b3b62008589a554b5") // (3)NeoSylphid_2.0
	{
		if (_mainPosition.y > mapH * .70 && _mainPosition.x > mapW * .50) _naturalChoke = BWAPI::Position(natLoc.x, natLoc.y - 100);
		else if (_mainPosition.y < mapH * .30)                                  _naturalChoke = BWAPI::Position(natLoc.x, natLoc.y + 200);
	}
	else if (mapHash == "ad870839912421dc3b4fd736a954bf770693ba9a") // (4)Polypoid_1.65
	{
		if (_mainPosition.y > mapH * .70 && _mainPosition.x > mapW * .50)
			_naturalChoke = BWAPI::Position(natLoc.x, natLoc.y - 150);
	}

	// Main choke (closest choke on path main->nat)
	shortest = 999999; distance = 0;
	if (_natural)
	{
		for (auto choke : mainToNat)
		{
			distance = abs((BWAPI::Position(choke->Center()).x - _natural->Center().x)
				- (BWAPI::Position(choke->Center()).y - _natural->Center().y));
			if (distance < shortest) { _mainChoke = BWAPI::Position(choke->Center()); shortest = distance; }
		}
	}

	if (mapHash == "c8386b87051f6773f6b2681b0e8318244aa086a6") // Neo Moon Glaive
	{
		if (_mainPosition.y < mapH / 2) _mainChoke += BWAPI::Position(200, 0);
	}

	// Small mineral tracking
	for (auto mineral : BWAPI::Broodwar->getStaticMinerals())
	{
		if (mineral->getInitialResources() >= 64) continue;
		length = -1;
		theMap.GetPath(mineral->getPosition(), BWAPI::Position(_mainPosition), &length);
		if (length < 0) _islandSmallMinerals.insert(mineral);
		else            _smallMinerals.insert(mineral);
	}

	// Scout rotation seeds
	for (auto & base : _otherBases)
	{
		_scanRotation.push_back(base.second->Location());
		_squadScoutRotation.push_back(base.second->Location());
	}

	// Neutral buildings
	for (auto unit : BWAPI::Broodwar->getStaticNeutralUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Special_Power_Generator ||
			unit->getType() == BWAPI::UnitTypes::Special_Protoss_Temple ||
			unit->getType() == BWAPI::UnitTypes::Special_XelNaga_Temple)
		{
			_neutralBuildings.push_back(unit);
		}
	}

	// Enemy race
	_enemyRace = (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Random)
		? BWAPI::Broodwar->enemy()->getRace()
		: BWAPI::Races::Unknown;

	// Bunker positions
	BWAPI::Position almostThere(
		_mainChoke.x + (BWAPI::Position(_mainPosition).x - _mainChoke.x) * .25f,
		_mainChoke.y + (BWAPI::Position(_mainPosition).y - _mainChoke.y) * .25f);
	_mainBunkerPos = BWAPI::TilePosition(almostThere);

	if (mapHash == "1e983eb6bcfa02ef7d75bd572cb59ad3aab49285") // Andromeda
		_mainBunkerPos += (_mainPosition.x < mapW * .30) ? BWAPI::TilePosition(-10, 0) : BWAPI::TilePosition(10, 0);
	else if (mapHash == "614d0048c6cc9dcf08da1409462f22f2ac4f5a0b") // PolarisRhapsody_1.0
		_mainBunkerPos += (_mainPosition.y > mapH * .50) ? BWAPI::TilePosition(0, -6) : BWAPI::TilePosition(2, 4);

	_NatBunkerPos = BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(getNaturalChokePos()))
		? BWAPI::TilePosition(getNaturalChokePos())
		: BWAPI::Broodwar->getBuildLocation(BWAPI::UnitTypes::Terran_Bunker, BWAPI::TilePosition(getNaturalChokePos()));
}

// ============================================================
// update: top-level frame routine, now a clean call sequence
// ============================================================

void InformationManager::update()
{
	updateAggression();
	updateNukeDotTracking();
	resolveEnemyBaseLocations();
	updateInitialScouting();
	updateRushDetection();
	updateBuildOrder();
	cleanDeadUnits();
	tryRegisterAllUnits();
	pruneEnemyStructures();
	checkLostEnemyBases();
	updateScans();
	cleanZombieTasks();
}

// ============================================================
// update() sub-routines
// ============================================================

void InformationManager::updateAggression()
{
	if (getAggression()) return;

	if (isBio(_strategy) &&
		((_marines.size() > 30 && _strategy != "BioDrops") ||
		(_strategy == "BioDrops" && _dropships.size() &&
			(_dropships.begin()->first->getDistance(getDropLocation(_dropships.begin()->first)) < 300 ||
				_marines.size() > 40))))
	{
		setAggression(true);
	}
	else if (isAirStrat(_strategy) && _bcs.size() >= 4)
	{
		setAggression(true);
	}
	else if (isMech(_strategy) && _tanks.size() >= 11)
	{
		setAggression(true);
	}
	else if (isAllIn(_strategy) && (_tanks.size() > 1 || (_strategy == "MechAllIn" && _vultures.size() > 6)))
	{
		setAggression(true);
	}
}

void InformationManager::updateNukeDotTracking()
{
	if (_nukeDotDetected)
	{
		if (BWAPI::Broodwar->getNukeDots().empty())
		{
			if (BWAPI::Broodwar->getFrameCount() - _waitASec > 150)
			{
				_nukeDotDetected = false;
				_targetDefended = false;
			}
		}
		else
		{
			_waitASec = BWAPI::Broodwar->getFrameCount();
		}
	}
	else if (!BWAPI::Broodwar->getNukeDots().empty())
	{
		_nukeDotDetected = true;
	}
}

void InformationManager::resolveEnemyBaseLocations()
{
	// Phase 1: resolve enemy main and nat from known enemy bases
	if (_enemyNatPos == BWAPI::Position(0, 0) && !_enemyBases.empty())
	{
		// Find which starting location corresponds to an enemy base
		for (auto startTile : BWAPI::Broodwar->getStartLocations())
		{
			for (auto & base : _enemyBases)
			{
				if (closeEnough(base.first, BWAPI::Position(startTile)))
				{
					_enemyMainPos = startTile;
					break;
				}
			}
			if (_enemyMainPos != BWAPI::TilePosition(0, 0)) break;
		}

		if (_enemyMainPos != BWAPI::TilePosition(0, 0) && _enemyNatPos == BWAPI::Position(0, 0))
		{
			// Search enemy bases first, then other bases
			auto findNat = [&](const std::map<BWAPI::Position, BWEM::Base *> & bases, int minLength)
			{
				int shortest = 999999, length = -1;
				for (auto & base : bases)
				{
					if (!base.second->baseHasGeyser()) continue;
					theMap.GetPath(BWAPI::Position(_enemyMainPos), base.first, &length);
					if (length < minLength) continue;
					if (length < shortest) { shortest = length; _enemyNatPos = base.first; }
				}
			};

			if (_enemyBases.size() > 1) findNat(_enemyBases, 1);
			if (_enemyNatPos == BWAPI::Position(0, 0)) findNat(_otherBases, 0);
		}
	}

	// Phase 2: resolve enemy nat choke once nat is known
	if (_enemyNatPos != BWAPI::Position(0, 0) && _enemyNatChoke == BWAPI::Position(0, 0))
	{
		int shortest = 999999, distance = 0, length = -1;

		for (auto & base : _otherBases)
		{
			if (closeEnough(base.first, _enemyNatPos) || closeEnough(base.first, BWAPI::Position(_enemyMainPos)))
				continue;

			BWEM::CPPath path = theMap.GetPath(base.first, _enemyNatPos, &length);
			if (length < 0) continue;

			for (auto choke : path)
			{
				distance = abs((BWAPI::Position(choke->Center()).x - _enemyNatPos.x)
					+ (BWAPI::Position(choke->Center()).y - _enemyNatPos.y));
				if (distance < shortest)
				{
					_enemyNatChoke = BWAPI::Position(choke->Center());
					shortest = distance;
				}
			}
			break;
		}
	}
}

void InformationManager::updateInitialScouting()
{
	if (_enemyBases.size() >= 1) return;
	if (_workers.size() < 7)    return;
	if (_scout && _scout->exists()) return;
	if (BWAPI::Broodwar->getFrameCount() >= 10000) return;
	if (_enemyRushing) return;

	for (auto it = _workers.begin(); it != _workers.end(); ++it)
	{
		if (!it->first || !it->first->exists()) continue;
		if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
		{
			BWAPI::Broodwar << it->first->getType() << " found in worker list." << std::endl;
			continue;
		}
		if (it->first->isIdle() || it->first->isGatheringMinerals())
		{
			if (it->first->isCarryingGas()) continue;
			if (it->second->isGasWorker(it->first)) continue;
			if (it->first->isConstructing()) continue;

			_scout = it->first;
			for (auto & base : _ownedBases)
			{
				if (base.second == it->second) { base.second->removeAssignment(it->first); break; }
			}
			_workers.erase(it);
			break;
		}
	}
}

void InformationManager::updateRushDetection()
{
	if (BWAPI::Broodwar->getFrameCount() < 5000)
		_enemyRushing = checkForEnemyRush();

	if (!_enemyRushing) return;
	if (!isTwoBasePlay(_strategy)) return;
	if (getOwnedBases().size() >= 2) return;
	if (_buildOrder->getInitialStrategy() == "MechAllIn") return;

	// Flush the queue and pivot strategy
	for (auto & item : _queue)
	{
		_reservedMinerals -= item.mineralPrice();
		_reservedGas -= item.gasPrice();
	}
	_queue.clear();

	_strategy = (_enemyRace == BWAPI::Races::Zerg) ? "8RaxDef" : "OneFacAllIn";
}

void InformationManager::cleanDeadUnits()
{
	_construction.clear();
	_injuredBuildings.clear();
	_floatingBuildings.clear();

	std::vector<std::reference_wrapper<std::map<BWAPI::Unit, int>>> unitMaps =
	{ _marines, _firebats, _medics, _ghosts, _vultures, _goliaths, _tanks, _vessels, _dropships, _bcs };

	std::vector<std::reference_wrapper<std::list<BWAPI::Unit>>> buildingLists =
	{ _slugDepots, _barracks, _factories, _starports, _science, _armories, _academy, _engibays, _bunkers, _comsats };

	for (auto & m : unitMaps)   checkForDeadMap(m);
	for (auto & l : buildingLists) checkForDeadList(l);
	checkForDeadTurrets(_turrets);

	if (!_commandCenters.empty()) checkForDeadCenters();
	if (!_refineries.empty())     checkForDeadRefineries();
	if (!_workers.empty())        checkForDeadWorkers();
}

void InformationManager::tryRegisterAllUnits()
{
	for (BWAPI::Unit unit : _self->getUnits())
	{
		if (!unit || !unit->exists()) continue;

		registerBuildingStates(unit);

		if (unit->getType().isWorker() && unit->getPlayer() == _self) tryRegisterWorker(unit);
		else if (unit->getPlayer() == _self)                          tryRegisterCombatUnit(unit) || tryRegisterBuilding(unit);
	}
}

void InformationManager::registerBuildingStates(BWAPI::Unit unit)
{
	if (!unit->getType().isBuilding()) return;

	if (unit->isBeingConstructed() || unit->canCancelConstruction())
	{
		_construction.push_back(unit);
	}
	else if (unit->isCompleted() && !unit->isFlying() &&
		(unit->getHitPoints() < unit->getType().maxHitPoints() / 2 ||
		(unit->getType().isResourceDepot() && !unit->isUnderAttack() &&
			unit->getHitPoints() < unit->getType().maxHitPoints())))
	{
		_injuredBuildings.push_back(unit);
	}
	else if (unit->isFlying())
	{
		_floatingBuildings.push_back(unit);
	}
}

bool InformationManager::tryRegisterWorker(BWAPI::Unit unit)
{
	// Already tracked somewhere?
	if (_workers.find(unit) != _workers.end())      return true;
	if (_islandWorkers.find(unit) != _islandWorkers.end()) return true;
	if (unit == _scout || unit == _islandBuilder)         return true;

	for (auto w : _repairWorkers) { if (w == unit) return true; }
	for (auto w : _bullyHunters) { if (w == unit) return true; }
	for (auto w : _fieldEngineers) { if (w == unit) return true; }

	assignWorkerToBase(unit);
	return true;
}

void InformationManager::assignWorkerToBase(BWAPI::Unit unit)
{
	// Try owned ground bases first
	for (auto & base : _ownedBases)
	{
		if (closeEnough(unit->getPosition(), base.first))
		{
			_workers.insert(std::make_pair(unit, base.second));
			return;
		}
	}

	// Try island bases
	if (!_ownedIslandBases.empty())
	{
		int length = -1;
		theMap.GetPath(unit->getPosition(), BWAPI::Position(_mainPosition), &length);

		int   closest = 9999999;
		BWEM::Base * island = _ownedIslandBases.begin()->second;

		for (auto & base : _ownedIslandBases)
		{
			if (closeEnough(unit->getPosition(), base.first))
			{
				_islandWorkers.insert(std::make_pair(unit, base.second));
				return;
			}
			if (length < 0 && unit->getDistance(base.first) < closest)
			{
				closest = unit->getDistance(base.first);
				island = base.second;
			}
		}

		if (closest < 9999999)
		{
			_islandWorkers.insert(std::make_pair(unit, island));
			return;
		}
	}

	// Fall back: assign to closest owned base
	if (_ownedBases.empty()) return;

	int closestDist = unit->getDistance(_ownedBases.begin()->first);
	BWEM::Base * assigned = _ownedBases.begin()->second;

	for (auto & base : _ownedBases)
	{
		int d = unit->getDistance(base.first);
		if (d < closestDist) { closestDist = d; assigned = base.second; }
	}
	_workers.insert(std::make_pair(unit, assigned));
}

bool InformationManager::tryRegisterCombatUnit(BWAPI::Unit unit)
{
	auto tryInsertMap = [&](std::map<BWAPI::Unit, int> & m) -> bool
	{
		if (m.find(unit) != m.end()) return true; // already tracked
		m.insert({ unit, 0 });
		return true;
	};

	const BWAPI::UnitType t = unit->getType();

	if (t == BWAPI::UnitTypes::Terran_Marine)                return tryInsertMap(_marines);
	else if (t == BWAPI::UnitTypes::Terran_Firebat)               return tryInsertMap(_firebats);
	else if (t == BWAPI::UnitTypes::Terran_Medic)                 return tryInsertMap(_medics);
	else if (t == BWAPI::UnitTypes::Terran_Ghost)                 return tryInsertMap(_ghosts);
	else if (t == BWAPI::UnitTypes::Terran_Dropship)              return tryInsertMap(_dropships);
	else if (t == BWAPI::UnitTypes::Terran_Science_Vessel)        return tryInsertMap(_vessels);
	else if (t == BWAPI::UnitTypes::Terran_Vulture)               return tryInsertMap(_vultures);
	else if (t == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)  return tryInsertMap(_tanks);
	else if (t == BWAPI::UnitTypes::Terran_Goliath)               return tryInsertMap(_goliaths);
	else if (t == BWAPI::UnitTypes::Terran_Battlecruiser)         return tryInsertMap(_bcs);

	return false;
}

bool InformationManager::tryRegisterBuilding(BWAPI::Unit unit)
{
	const BWAPI::UnitType t = unit->getType();

	// Helper: avoid duplicate list entries, call checkQueue on first registration
	auto tryInsertList = [&](std::list<BWAPI::Unit> & l, bool callCheckQueue = true) -> bool
	{
		if (std::find(l.begin(), l.end(), unit) != l.end()) return true;
		l.push_back(unit);
		if (callCheckQueue) checkQueue(unit);
		return true;
	};

	if (t == BWAPI::UnitTypes::Terran_Supply_Depot)       return tryInsertList(_slugDepots);
	if (t == BWAPI::UnitTypes::Terran_Barracks)           return tryInsertList(_barracks);
	if (t == BWAPI::UnitTypes::Terran_Factory)            return tryInsertList(_factories);
	if (t == BWAPI::UnitTypes::Terran_Starport)           return tryInsertList(_starports);
	if (t == BWAPI::UnitTypes::Terran_Academy)            return tryInsertList(_academy);
	if (t == BWAPI::UnitTypes::Terran_Engineering_Bay)    return tryInsertList(_engibays);
	if (t == BWAPI::UnitTypes::Terran_Armory)             return tryInsertList(_armories);
	if (t == BWAPI::UnitTypes::Terran_Science_Facility)   return tryInsertList(_science);
	if (t == BWAPI::UnitTypes::Terran_Bunker)             return tryInsertList(_bunkers);

	if (t == BWAPI::UnitTypes::Terran_Missile_Turret)
	{
		if (std::find(_turrets.begin(), _turrets.end(), unit) != _turrets.end()) return true;
		for (auto & base : _ownedBases)
			if (unit->getDistance(base.first) < 400) { base.second->addTurrets(unit); break; }
		for (auto & base : _ownedIslandBases)
			if (unit->getDistance(base.first) < 400) { base.second->addTurrets(unit); break; }
		_turrets.push_back(unit);
		checkQueue(unit);
		return true;
	}

	if (t.isAddon())
	{
		if (t == BWAPI::UnitTypes::Terran_Comsat_Station)  return tryInsertList(_comsats);
		if (t == BWAPI::UnitTypes::Terran_Machine_Shop)    return tryInsertList(_machineShops);
		return tryInsertList(_addons);
	}

	if (t.isResourceDepot())
	{
		if (std::find(_commandCenters.begin(), _commandCenters.end(), unit) != _commandCenters.end()) return true;
		if (std::find(_islandCenters.begin(), _islandCenters.end(), unit) != _islandCenters.end()) return true;

		// Ground expansion
		for (auto it = _otherBases.begin(); it != _otherBases.end(); ++it)
		{
			if (closeEnough(unit->getPosition(), it->first))
			{
				it->second->setbaseCommandCenter(unit);
				_ownedBases.insert({ it->first, it->second });
				_otherBases.erase(it);
				break;
			}
		}

		// Island expansion
		bool isIsland = false;
		for (auto it = _islandBases.begin(); it != _islandBases.end(); ++it)
		{
			if (closeEnough(unit->getPosition(), it->first))
			{
				if (_islandBuilder && _islandBuilder->exists())
				{
					_islandWorkers.insert({ _islandBuilder, it->second });
					_islandBuilder = NULL;
				}
				_islandCenters.push_back(unit);
				isIsland = true;
				it->second->setbaseCommandCenter(unit);
				_ownedIslandBases.insert({ it->first, it->second });
				_islandBases.erase(it);
				break;
			}
		}

		if (!isIsland) _commandCenters.push_back(unit);
		checkQueue(unit);
		return true;
	}

	if (t.isRefinery())
	{
		if (std::find(_refineries.begin(), _refineries.end(), unit) != _refineries.end()) return true;

		auto matchGeyser = [&](std::map<BWAPI::Position, BWEM::Base *> & bases) -> bool
		{
			for (auto & base : bases)
			{
				if (!base.second->baseHasGeyser()) continue;
				auto tp = BWAPI::TilePosition(unit->getPosition());
				auto gl = base.second->Geysers().front()->TopLeft();
				if (abs(tp.x - gl.x) <= 8 && abs(tp.y - gl.y) <= 8)
				{
					bases[base.first]->setBaseRefinery(unit);
					return true;
				}
			}
			return false;
		};

		matchGeyser(_ownedBases);
		matchGeyser(_ownedIslandBases);
		_refineries.push_back(unit);
		checkQueue(unit);
		return true;
	}

	return false;
}

void InformationManager::pruneEnemyStructures()
{
	for (auto it = _enemyStructurePositions.begin(); it != _enemyStructurePositions.end(); )
	{
		BWAPI::TilePosition tp(it->x / 32, it->y / 32);
		if (!BWAPI::Broodwar->isVisible(tp)) { ++it; continue; }

		bool stillThere = false;
		for (auto u : _enemy->getUnits())
		{
			if (!u || !u->exists()) continue;
			if (u->getType().isBuilding() && u->getPosition() == *it) { stillThere = true; break; }
		}

		it = stillThere ? std::next(it) : _enemyStructurePositions.erase(it);
	}
}

void InformationManager::checkLostEnemyBases()
{
	for (auto & base : _enemyBases)
	{
		if (!BWAPI::Broodwar->isVisible(base.second->Location())) continue;

		bool exists = false;
		for (auto unit : _enemy->getUnits())
		{
			if (unit && unit->exists() && unit->getType().isResourceDepot() &&
				closeEnough(unit->getPosition(), base.first))
			{
				exists = true;
				break;
			}
		}

		if (!exists)
		{
			base.second->setbaseCommandCenter(NULL);
			base.second->clearAssignmentList();
			_otherBases.insert({ base.first, base.second });
			_enemyBases.erase(base.first);
			if (_targetDefended) _targetDefended = false;
			break;
		}
	}
}

void InformationManager::updateScans()
{
	if (_comsats.empty()) return;

	bool scanUsed = false;

	// Priority scan: visible but undetected enemy unit
	for (auto unit : _enemy->getUnits())
	{
		if (!unit) continue;
		if (!unit->isVisible() || unit->isDetected()) continue;
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer) continue;

		for (auto com : _comsats)
		{
			if (!com || !com->exists()) continue;
			if (com->getEnergy() >= BWAPI::TechTypes::Scanner_Sweep.energyCost() &&
				com->getPlayer() == _self &&
				BWAPI::Broodwar->getFrameCount() - _lastScanFrame > 200)
			{
				_lastScanFrame = BWAPI::Broodwar->getFrameCount();
				com->useTech(BWAPI::TechTypes::Scanner_Sweep, unit->getPosition());
				scanUsed = true;
				break;
			}
		}
		if (scanUsed) break;
	}

	if (scanUsed) return;

	// Rotation scan: energy-flush to prevent waste
	if (BWAPI::Broodwar->getFrameCount() - _lastScanFrame <= 150) return;

	for (auto com : _comsats)
	{
		if (!com || !com->exists() || com->getPlayer() != _self) continue;
		if (com->getEnergy() < 195) continue;

		if (!BWAPI::Broodwar->isVisible(_enemyMainPos) &&
			BWAPI::Broodwar->getFrameCount() - _lastScanFrame > 1500)
		{
			_lastScanFrame = BWAPI::Broodwar->getFrameCount();
			com->useTech(BWAPI::TechTypes::Scanner_Sweep, BWAPI::Position(_enemyMainPos));
			break;
		}

		BWAPI::TilePosition nextUp = _scanRotation.front();
		if (!BWAPI::Broodwar->isVisible(nextUp.x, nextUp.y))
			com->useTech(BWAPI::TechTypes::Scanner_Sweep, BWAPI::Position(nextUp));

		_scanRotation.push_back(_scanRotation.front());
		_scanRotation.erase(_scanRotation.begin());
		break;
	}
}

void InformationManager::cleanZombieTasks()
{
	if (BWAPI::Broodwar->getFrameCount() % 10 != 0) return;
	for (auto & base : _ownedBases) base.second->cleanUpZombieTasks();
}

// ============================================================
// Dead unit cleanup
// ============================================================

void InformationManager::checkForDeadList(std::list<BWAPI::Unit> & listToDeleteFrom)
{
	if (listToDeleteFrom.empty()) return;
	listToDeleteFrom.remove_if([](const BWAPI::Unit & u) { return !u || !u->exists(); });
}

void InformationManager::checkForDeadMap(std::map<BWAPI::Unit, int> & mapToDeleteFrom)
{
	if (mapToDeleteFrom.empty()) return;

	std::vector<BWAPI::Unit> dead;
	for (auto & kv : mapToDeleteFrom)
		if (!kv.first || !kv.first->exists()) dead.push_back(kv.first);
	for (auto u : dead) mapToDeleteFrom.erase(u);
}

void InformationManager::checkForDeadTurrets(std::list<BWAPI::Unit> & listToDeleteFrom)
{
	std::vector<BWAPI::Unit> dead;

	for (const auto & unit : listToDeleteFrom)
	{
		if (!unit || !unit->exists())
		{
			dead.push_back(unit);

			// Notify all base groups so turret counts stay accurate
			for (auto * bases : { &_ownedBases, &_ownedIslandBases, &_otherBases, &_enemyBases, &_islandBases })
			{
				bool found = false;
				for (auto & base : *bases)
				{
					if (base.second->onTurretDestroy()) { found = true; break; }
				}
				if (found) break;
			}
		}
	}

	for (const auto & u : dead) listToDeleteFrom.remove(u);
}

void InformationManager::checkForDeadWorkers()
{
	for (auto worker = _workers.begin(); worker != _workers.end(); )
	{
		if (!worker->first || !worker->first->exists())
		{
			for (auto & base : _ownedBases)       base.second->onUnitDestroy(worker->first);
			for (auto & base : _ownedIslandBases) base.second->onUnitDestroy(worker->first);
			worker = _workers.erase(worker);
		}
		else
		{
			++worker;
		}
	}
}

void InformationManager::checkForDeadCenters()
{
	for (auto center = _commandCenters.begin(); center != _commandCenters.end(); )
	{
		if (!(*center) || !(*center)->exists())
		{
			// Ground bases
			for (auto base = _ownedBases.begin(); base != _ownedBases.end(); ++base)
			{
				if (!base->second->getBaseCommandCenter() || !base->second->getBaseCommandCenter()->exists())
				{
					for (auto & worker : _workers)
					{
						if (!worker.first || !worker.first->exists()) continue;
						if (worker.second == base->second)
							worker.second = _ownedBases.empty() ? _home : _ownedBases.begin()->second;
						if (_ownedBases.empty()) worker.first->move(_home->Center());
					}

					// Drop any queued refinery for this dead base
					for (auto it = _queue.begin(); it != _queue.end(); ++it)
					{
						if (*it == BWAPI::UnitTypes::Terran_Refinery)
						{
							_queue.erase(it);
							if (_reservedMinerals >= 100) _reservedMinerals -= 100;
							break;
						}
					}

					base->second->setbaseCommandCenter(NULL);
					base->second->clearAssignmentList();
					_otherBases.insert({ base->first, base->second });
					_ownedBases.erase(base->first);
					break;
				}
			}

			// Island bases
			if (!_ownedIslandBases.empty())
			{
				for (auto base = _ownedIslandBases.begin(); base != _ownedIslandBases.end(); ++base)
				{
					if (!closeEnough((*center)->getPosition(), base->first)) continue;

					for (auto & worker : _islandWorkers)
					{
						if (!worker.first || !worker.first->exists()) continue;
						if (worker.second == base->second)
						{
							worker.second = _ownedIslandBases.empty() ? _home : _ownedIslandBases.begin()->second;
							if (_ownedIslandBases.empty()) worker.first->move(_home->Center());
						}
					}

					_islandCenters.remove_if([&](BWAPI::Unit u) { return u == *center; });

					base->second->setbaseCommandCenter(NULL);
					base->second->clearAssignmentList();
					_islandBases.insert({ base->first, base->second });
					_ownedIslandBases.erase(base->first);
					break;
				}
			}

			center = _commandCenters.erase(center);
		}
		else
		{
			++center;
		}
	}
}

void InformationManager::checkForDeadRefineries()
{
	for (auto refinery = _refineries.begin(); refinery != _refineries.end(); )
	{
		bool isDead = !(*refinery) || !(*refinery)->exists() ||
			((*refinery)->exists() && (*refinery)->getType() != BWAPI::UnitTypes::Terran_Refinery);

		if (isDead)
		{
			auto clearRefinery = [&](std::map<BWAPI::Position, BWEM::Base *> & bases)
			{
				for (auto & base : bases)
				{
					if (!base.second->Geysers().size()) continue;
					auto tp = BWAPI::TilePosition((*refinery)->getPosition());
					auto gl = base.second->Geysers().front()->TopLeft();
					if (abs(tp.x - gl.x) <= 8 && abs(tp.y - gl.y) <= 8)
					{
						bases[base.first]->setBaseRefinery(NULL);
						break;
					}
				}
			};

			clearRefinery(_ownedBases);
			clearRefinery(_otherBases);
			clearRefinery(_islandBases);
			clearRefinery(_ownedIslandBases);

			refinery = _refineries.erase(refinery);
		}
		else
		{
			++refinery;
		}
	}
}

// ============================================================
// checkQueue
// ============================================================

void InformationManager::checkQueue(BWAPI::Unit myUnit)
{
	for (auto it = _queue.begin(); it != _queue.end(); ++it)
	{
		if (*it == myUnit->getType())
		{
			_reservedMinerals -= myUnit->getType().mineralPrice();
			_reservedGas -= myUnit->getType().gasPrice();
			_queue.erase(it);
			return;
		}
	}
}

// ============================================================
// updateBuildOrder
// ============================================================

void InformationManager::updateBuildOrder()
{
	if (hasInitialBarracks() && _barracks.empty() && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Barracks))
		setInitialBarracks(false);

	if (!_queue.empty()) return;

	// Dispatch to the correct build order function
	auto it = _buildOrderDispatch.find(_strategy);
	if (it != _buildOrderDispatch.end())
		it->second(*_buildOrder, *this);

	// Reactive additions on top of the build order

	int extraCover = (_enemyRace == BWAPI::Races::Zerg && _enemyHasAir) ? 3 : 0;

	if (_enemyHasDtLurker)
	{
		extraCover += std::min(static_cast<int>(_bunkers.size()), 2);
		if (_engibays.empty() &&
			std::find(_queue.begin(), _queue.end(), BWAPI::UnitTypes::Terran_Engineering_Bay) == _queue.end())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
			_reservedMinerals += BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice();
		}
	}

	if (_enemyHasAir && _engibays.empty() &&
		std::find(_queue.begin(), _queue.end(), BWAPI::UnitTypes::Terran_Engineering_Bay) == _queue.end())
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
		_reservedMinerals += BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice();
	}

	int numTurretsWanted = (getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center) - (int)_ownedIslandBases.size()) * 3 + extraCover;

	if (getNumFinishedUnit(BWAPI::UnitTypes::Terran_Engineering_Bay) &&
		getNumTotalUnit(BWAPI::UnitTypes::Terran_Missile_Turret) < numTurretsWanted &&
		((_enemyRace == BWAPI::Races::Zerg || _enemyHasAir) || extraCover == 1))
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Missile_Turret);
		_reservedMinerals += BWAPI::UnitTypes::Terran_Missile_Turret.mineralPrice();
	}
}

// ============================================================
// Event handlers
// ============================================================

void InformationManager::onUnitShow(BWAPI::Unit unit)
{
	if (!unit || !unit->exists()) return;
	if (unit->getPlayer() != _enemy) return;

	// Race identification
	if (_enemyRace == BWAPI::Races::Unknown)
	{
		_enemyRace = unit->getType().getRace();
		if (_enemyRace == BWAPI::Races::Zerg && !isBio(_strategy))
		{
			_strategy = "Nuke";
			_buildOrder->setInitialStrategy(_strategy);
		}
	}

	// Air threat detection
	if (!_enemyHasAir && unit->isFlying() && !unit->getType().isBuilding() &&
		unit->getType() != BWAPI::UnitTypes::Zerg_Overlord)
	{
		_enemyHasAir = true;
	}

	// DT/Lurker threat detection
	if (!_enemyHasDtLurker)
	{
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Lurker_Egg ||
			(_enemyLair && _enemyHydraDen) ||
			((unit->getType() == BWAPI::UnitTypes::Protoss_Templar_Archives ||
				unit->getType() == BWAPI::UnitTypes::Protoss_Citadel_of_Adun) &&
				BWAPI::Broodwar->getFrameCount() < 8000))
		{
			_enemyHasDtLurker = true;
		}
	}

	// FiveFacGol aggression reset on sunken sighting
	if (_strategy == "FiveFacGol" && _attack && _tanks.empty() &&
		unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony)
	{
		_attack = false;
	}

	if (unit->getType().isBuilding())
	{
		// Track enemy structure positions
		bool found = false;
		for (auto & pos : _enemyStructurePositions)
			if (pos == unit->getPosition()) { found = true; break; }
		if (!found) _enemyStructurePositions.push_back(unit->getPosition());

		if (!_enemyPool     && unit->getType() == BWAPI::UnitTypes::Zerg_Spawning_Pool)     _enemyPool = true;
		if (!_enemyCyberCore && unit->getType() == BWAPI::UnitTypes::Protoss_Cybernetics_Core) _enemyCyberCore = true;
		if (!_enemyHydraDen && unit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk_Den)      _enemyHydraDen = true;
		if (!_enemyLair && (unit->getType() == BWAPI::UnitTypes::Zerg_Lair ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Hive)) _enemyLair = true;
	}

	if (unit->getType().isResourceDepot())
	{
		// Try ground bases first
		auto tryAddEnemyBase = [&](std::map<BWAPI::Position, BWEM::Base *> & bases) -> bool
		{
			for (auto & base : bases)
			{
				if (closeEnough(unit->getPosition(), base.first))
				{
					base.second->setbaseCommandCenter(unit);
					_enemyBases.insert({ base.first, base.second });
					bases.erase(base.first);
					return true;
				}
			}
			return false;
		};

		if (tryAddEnemyBase(_otherBases)) return;
		if (tryAddEnemyBase(_islandBases)) return;
	}
	else
	{
		if (!_enemyUnits.contains(unit)) _enemyUnits.insert(unit);
	}
}

void InformationManager::onUnitDestroy(BWAPI::Unit unit)
{
	if (unit->getType().isMineralField())
	{
		_smallMinerals.erase(unit);
		_islandSmallMinerals.erase(unit);

		for (auto & base : _ownedBases)
			if (auto * m = base.second->getDestroyedMineral(unit)) base.second->OnMineralDestroyed(m);
		for (auto & base : _otherBases)
			if (auto * m = base.second->getDestroyedMineral(unit)) base.second->OnMineralDestroyed(m);
		for (auto & base : _islandBases)
			if (auto * m = base.second->getDestroyedMineral(unit)) base.second->OnMineralDestroyed(m);
		for (auto & base : _ownedIslandBases)
			if (auto * m = base.second->getDestroyedMineral(unit)) base.second->OnMineralDestroyed(m);
	}
	else if (unit->getPlayer() == BWAPI::Broodwar->neutral() && unit->getType().isBuilding())
	{
		_neutralBuildings.remove(unit);
	}
}

void InformationManager::onUnitRenegade(BWAPI::Unit unit)
{
	if (!unit || !unit->exists()) return;

	auto assignRefinery = [&](std::map<BWAPI::Position, BWEM::Base *> & bases) -> bool
	{
		for (auto & base : bases)
		{
			if (!base.second->baseHasGeyser()) continue;
			auto tp = BWAPI::TilePosition(unit->getPosition());
			auto gl = base.second->Geysers().front()->TopLeft();
			if (abs(tp.x - gl.x) <= 8 && abs(tp.y - gl.y) <= 8)
			{
				bases[base.first]->setBaseRefinery(unit);
				return true;
			}
		}
		return false;
	};

	if (unit->getType().isRefinery() && unit->getPlayer() == _self)
	{
		if (!assignRefinery(_ownedBases) && !assignRefinery(_ownedIslandBases))
			BWAPI::Broodwar << "No base found for built geyser" << std::endl;
	}
	else if (unit->getType().isRefinery() && unit->getPlayer() == _enemy)
	{
		assignRefinery(_enemyBases);
		assignRefinery(_otherBases);
	}
}

void InformationManager::onUnitComplete(BWAPI::Unit unit)
{
	// Reserved for future use
}

// ============================================================
// Expansion / squad queries
// ============================================================

bool InformationManager::shouldExpand()
{
	if (_otherBases.empty() || (_strategy == "Mech" && _tanks.size() < 8)) return false;

	int baseAbove = 0;
	for (auto & base : _ownedBases)
		if (base.second->getRemainingMinerals() > 1500) ++baseAbove;

	if (isBio(_strategy) && baseAbove < 2) return true;
	if (isMech(_strategy) && baseAbove < 2) return true;
	if (isAllIn(_strategy) && baseAbove < 1) return true;
	return false;
}

bool InformationManager::isExpanding()
{
	for (auto & item : _queue)
		if (item.isResourceDepot()) return true;
	return false;
}

bool InformationManager::shouldHaveDefenseSquad(bool worker)
{
	int defenceRadius = (std::find(_smallMainMaps.begin(), _smallMainMaps.end(), BWAPI::Broodwar->mapHash()) != _smallMainMaps.end()) ? 500 : 800;

	for (auto unit : _enemy->getUnits())
	{
		if (!unit) continue;
		if (unit->exists() &&
			unit->getDistance(BWAPI::Position(_mainPosition)) < defenceRadius &&
			unit->getType() != BWAPI::UnitTypes::Zerg_Overlord &&
			unit->getType() != BWAPI::UnitTypes::Protoss_Observer)
			return true;
	}

	if (BWAPI::Broodwar->getFrameCount() > 20000 && !worker)
	{
		for (auto & base : _ownedBases)
			for (auto structure : _neutralBuildings)
				if (structure->getDistance(base.first) < 800) return true;
	}

	return false;
}

int InformationManager::numFrontierSquadsNeeded()
{
	int numSquadsWanted = (int)_ownedBases.size() - 2;
	numSquadsWanted += (isExpanding() && !_islandExpand ? 1 : 0);

	// For bio builds, don't count outer bases that already have a filled bunker
	if (isBio(getStrategy()))
	{
		const double bunkerSearchRadius = 512.0;
		for (auto & base : _ownedBases)
		{
			if (closeEnough(base.first, _home->Center()) || closeEnough(base.first, _natural->Center()))
				continue;

			for (auto bunker : getBunkers())
			{
				if (!bunker || !bunker->exists() || bunker->isBeingConstructed())
					continue;
				if (bunker->getDistance(base.first) < bunkerSearchRadius &&
					bunker->getLoadedUnits().size() == bunker->getType().spaceProvided())
				{
					numSquadsWanted--;
					break;
				}
			}
		}
	}

	return std::max(numSquadsWanted, 0);
}

// ============================================================
// Rush detection
// ============================================================

bool InformationManager::checkForEnemyRush()
{
	if (_enemyRace == BWAPI::Races::Zerg)
	{
		int numWorker = 0;
		for (auto unit : _enemyUnits)
			if (unit->getType().isWorker()) ++numWorker;
		if (numWorker > _enemyWorkerNumber) _enemyWorkerNumber = numWorker;

		if (_enemyPool && _enemyWorkerNumber <= 5 && _enemyBases.size() < 2 && _enemyMainPos != BWAPI::TilePosition(0, 0))
			return true;

		if (!_enemyUnits.empty())
		{
			int numHatch = 0, numZerglings = 0;
			for (auto unit : _enemyUnits)
			{
				if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling) ++numZerglings;
				else if (unit->getType().isResourceDepot())              ++numHatch;
			}
			if (numHatch > 1 || _enemyBases.size() > 1) return false;
			if (numZerglings >= 6)                       return true;
		}
	}
	else if (_enemyRace == BWAPI::Races::Protoss)
	{
		if (!_enemyUnits.empty())
		{
			int numGateways = 0, numNexus = 0;
			for (auto unit : _enemyUnits)
			{
				if (!unit || !unit->exists()) continue;
				if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus)   ++numNexus;
				else if (unit->getType() == BWAPI::UnitTypes::Protoss_Gateway) ++numGateways;
				else if (unit->getType() == BWAPI::UnitTypes::Protoss_Pylon && unit->getDistance(_mainChoke) < 1500) return true;
			}
			if (numNexus > 1 || _enemyBases.size() > 1)   return false;
			if (numGateways >= 2 && !_enemyCyberCore)      return true;
		}
	}
	else if (_enemyRace == BWAPI::Races::Terran)
	{
		if (!_enemyUnits.empty())
		{
			int numRax = 0, numCC = 0;
			for (auto unit : _enemyUnits)
			{
				if (unit->getType() == BWAPI::UnitTypes::Terran_Command_Center) ++numCC;
				else if (unit->getType() == BWAPI::UnitTypes::Terran_Barracks)       ++numRax;
			}
			if (numCC > 1)    return false;
			if (numRax >= 2)  return true;
		}
	}
	return false;
}

// ============================================================
// Utility / getters
// ============================================================

std::vector<BWAPI::Unit> InformationManager::mineralsNeedClear()
{
	std::vector<BWAPI::Unit> result;
	for (auto & base : _ownedBases)
		for (auto mineral : _smallMinerals)
			if (mineral->getDistance(base.first) < 1000) result.push_back(mineral);
	return result;
}

bool InformationManager::closeEnough(BWAPI::Position a, BWAPI::Position b)
{
	return abs(a.x - b.x) <= 64 && abs(a.y - b.y) <= 64;
}

int InformationManager::getNumFinishedUnit(BWAPI::UnitType type)
{
	int count = 0;
	for (auto & u : _self->getUnits())
		if (u && u->getType() == type && u->isCompleted() && u->exists()) ++count;
	return count;
}

int InformationManager::getNumUnfinishedUnit(BWAPI::UnitType type)
{
	int count = 0;
	for (auto & u : _self->getUnits())
		if (u && u->getType() == type && !u->isCompleted() && u->exists()) ++count;
	return count;
}

int InformationManager::getNumTotalUnit(BWAPI::UnitType type)
{
	int count = 0;
	for (auto & u : _self->getUnits())
		if (u && u->getType() == type && u->exists()) ++count;
	return count;
}

int InformationManager::numGeyserBases()
{
	int count = 0;
	for (auto & base : _ownedBases) count += base.second->baseHasGeyser();
	return count;
}

int InformationManager::numWorkersWanted()
{
	int numWanted = 0;
	for (auto & base : _ownedBases) numWanted += base.second->numWorkersWantedHere();
	if (numWanted > 70) return 70;
	if (numWanted < 3)  return 3 * getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center);
	return numWanted;
}

bool InformationManager::targetIsDefended()
{
	if (_strategy != "Nuke" || _enemyBases.empty()) return false;
	if (_targetDefended) return true;

	BWAPI::Position target = _enemyBases.begin()->first;
	if (closeEnough(target, BWAPI::Position(_enemyMainPos)))
	{
		for (auto & base : _enemyBases)
		{
			if (closeEnough(base.first, _enemyNatPos)) { target = base.first; break; }
		}
	}

	int numSunks = 0;
	for (auto unit : _enemy->getUnits())
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony && !unit->isMorphing() && unit->getDistance(target) < 800)
			++numSunks;

	if (numSunks > 1) { _targetDefended = true; return true; }
	return false;
}

BWAPI::Position InformationManager::getAirGatherLocation()
{
	BWAPI::TilePosition center(BWAPI::Broodwar->mapWidth() / 2, BWAPI::Broodwar->mapHeight() / 2);
	BWAPI::TilePosition edge(BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

	if (_enemyMainPos != BWAPI::TilePosition(0, 0) && _enemyMainPos.x < BWAPI::Broodwar->mapWidth() * .30)
		return BWAPI::Position(100, BWAPI::Position(center).y);
	if (_enemyMainPos != BWAPI::TilePosition(0, 0))
		return BWAPI::Position(BWAPI::Position(edge).x - 100, BWAPI::Position(center).y);

	return BWAPI::Position(_mainPosition);
}

BWAPI::Position InformationManager::getDropLocation(BWAPI::Unit dropship)
{
	if (_strategy == "BioDrops") return BWAPI::Position(_enemyMainPos);

	BWAPI::Position target = BWAPI::Position(_enemyMainPos);
	bool hasMain = false;

	for (auto & base : _enemyBases)
	{
		if (!hasMain && closeEnough(BWAPI::Position(_enemyMainPos), base.first)) hasMain = true;
		if (dropship->getDistance(base.first) < 300) target = base.first;
	}

	if (!hasMain && target == BWAPI::Position(_enemyMainPos) && !_enemyBases.empty())
		target = _enemyBases.begin()->first;

	return target;
}

int InformationManager::numLoadedDropsWanted()
{
	if (isMech(_strategy))    return 2;
	if (_strategy == "BioDrops") return 4;
	return 1;
}

void InformationManager::onGameEnd(bool isWinner)
{
	_readWrite->writeCompactMatchData(
		BWAPI::Broodwar->enemy()->getName(),
		BWAPI::Broodwar->mapFileName(),
		_mainNotTilePos,
		getInitialStrategy(),
		isWinner);
}
