#include "InformationManager.h"
#include <time.h>


namespace { auto & theMap = BWEM::Map::Instance(); }

using namespace insanitybot;

insanitybot::InformationManager::InformationManager()
	:	_self(BWAPI::Broodwar->self())
	, _enemy(BWAPI::Broodwar->enemy())
{
	_attack = false;
	_islandExpand = false;
	_lastScanFrame = 0;
	_enemyHasAir = false;
	_enemyRushing = false;
	_enemyPool = false;
	_targetDefended = false;
	_nukeDotDetected = false;
	_waitASec = 0;

	std::srand(time(NULL));
	int _randomChoice = std::rand() % 2;

	if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
	{
		_strategy = "Nuke";
	}
	else if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss)
	{
		switch (_randomChoice)
		{
		case 1:
			_strategy = "OneFacAllIn";
			break;
		default:
			_strategy = "Mech";
			break;
		}

		BWAPI::Broodwar << "Enemy: " << _enemy->getName() << std::endl;
		if (_enemy->getName() == "Tomas Vajda")
		{
			_strategy = "OneFacAllIn";
		}
	}
	else if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran)
	{
		switch (_randomChoice)
		{
		case 1:
			_strategy = "MechAllIn";
			break;
		default:
			_strategy = "Mech";
			break;
		}
	}
	else // Random
	{
		switch (_randomChoice)
		{
		case 1:
			_strategy = "oneFacAllIn";
			break;
		default:
			_strategy = "Mech";
			break;
		}
	}
}

/****************************************************
* initialize will setup all of the basic information 
* we need for base positions, initial workers and command
* centers, and chokepoint locations.
*****************************************************/
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

	// Add our initial command center's location as our main location
	for (auto &u : BWAPI::Broodwar->self()->getUnits())
	{
		if (!u)
			continue;

		if (u->getType().isResourceDepot())
		{
			//BWAPI::Broodwar << "Main Position at: " << u->getTilePosition() << std::endl;
			main = u->getPosition();
			setMainPosition(u->getTilePosition());
			break;
		}
	}

	// Loop through all of the bases on the map and add them to our lists
	for (const BWEM::Area & area : theMap.Areas())
	{
		for (const BWEM::Base & base : area.Bases())
		{
			// If it is our main base, add it to our list of owned bases
			if (closeEnough(BWAPI::Position(base.Location()), main))
			{
				std::vector<BWEM::Ressource *> assignedResources;
				for (BWEM::Mineral * m : base.Minerals())
				{
					assignedResources.push_back(m);
				}
				for (BWEM::Geyser * g : base.Geysers())
				{
					assignedResources.push_back(g);

				}
				BWEM::Area _homeArea = area;
				_home = new BWEM::Base(&_homeArea, base.Location(), assignedResources, base.BlockingMinerals());
				_ownedBases[BWAPI::Position(base.Location())] = new BWEM::Base(&_homeArea, base.Location(), assignedResources, base.BlockingMinerals());

			}
			// If it is another base, add it to our list of other bases
			else
			{
				std::vector<BWEM::Ressource *> assignedResources;
				for (BWEM::Mineral * m : base.Minerals())
				{
					assignedResources.push_back(m);
				}
				for (BWEM::Geyser * g : base.Geysers())
				{
					assignedResources.push_back(g);

				}
				BWEM::Area _otherBaseArea = area;

				int length = -1;
				theMap.GetPath(BWAPI::Position(base.Location()), BWAPI::Position(_mainPosition), &length);

				if (length < 0)
				{
					_islandBases[BWAPI::Position(base.Location())] = new BWEM::Base(&_otherBaseArea, base.Location(), assignedResources, base.BlockingMinerals());
				}
				else
				{
					_otherBases[BWAPI::Position(base.Location())] = new BWEM::Base(&_otherBaseArea, base.Location(), assignedResources, base.BlockingMinerals());
				}
			}
		}
	}

	// Insert our initial workers into our worker list
	for (auto &u : BWAPI::Broodwar->self()->getUnits())
	{
		if (!u || !u->exists())
			continue;

		if (u->getType().isWorker())
		{
			//_workers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(u, _ownedBases.begin()->second));
			_workers.insert(std::make_pair(u, _ownedBases.begin()->second));
		}
		else if (u->getType().isResourceDepot())
		{
			_commandCenters.push_back(u);
		}
	}

	// Setup our natural and main choke locations as well as natural base location
	int shortest = 999999;
	int length = -1;
	BWEM::CPPath mainToNat;
	for (auto base : _otherBases)
	{
		if (base.second->baseHasGeyser())
		{
			BWEM::CPPath temp = theMap.GetPath(BWAPI::Position(base.first), BWAPI::Position(_mainPosition), &length);

			if (length < 0) continue;

			if (length < shortest)
			{
				mainToNat = temp;
				shortest = length;
				_natural = base.second;
			}
		}
	}

	// This doesn't account for mineral only bases on maps that have essentially a second natural expansion like andromeda/electric circuit.
	// As it stands, it doesn't seem to affect any of the standard maps other than electric circuit so I'm not going to worry about it.
	shortest = 999999;
	int distance = 0;
	length = -1;
	for (auto base : _otherBases)
	{
		if (base.second == _natural)
			continue;

		BWEM::CPPath natToSomewhere = theMap.GetPath(BWAPI::Position(base.first), BWAPI::Position(_natural->Center()), &length);

		if (length < 0) continue;

		for (auto choke : natToSomewhere)
		{
			distance = abs((BWAPI::Position(choke->Center()).x - _natural->Center().x) - (BWAPI::Position(choke->Center()).y - _natural->Center().y));

			if (distance < shortest)
			{
				_naturalChoke = BWAPI::Position(BWAPI::Position(choke->Center()).x, BWAPI::Position(choke->Center()).y);
				shortest = distance;
			}
		}

		break;
	}

	// Slight adjustments to natural choke positions
	if (BWAPI::Broodwar->mapHash() == "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b") // Destination
	{
		// If our base is on the bottom of the map
		if (_mainPosition.y > BWAPI::Broodwar->mapHeight() / 2)
			_naturalChoke += BWAPI::Position(-150, 200);
		else // On top
			_naturalChoke += BWAPI::Position(-100, -150);
	}
	else if (BWAPI::Broodwar->mapHash() == "c8386b87051f6773f6b2681b0e8318244aa086a6") // Neo Moon Glaive
	{
		// If our base is on the top of the map
		if (_mainPosition.y < BWAPI::Broodwar->mapHeight() / 2)
			_naturalChoke += BWAPI::Position(300, 0);
	}
	else if (BWAPI::Broodwar->mapHash() == "d2f5633cc4bb0fca13cd1250729d5530c82c7451") // Fighting Spirit
	{
		if (_mainPosition.y < BWAPI::Broodwar->mapHeight() / 2 && _mainPosition.x > BWAPI::Broodwar->mapWidth() / 2) // Top Right
			_naturalChoke += BWAPI::Position(50, -200);
	}
	else if (BWAPI::Broodwar->mapHash() == "9a4498a896b28d115129624f1c05322f48188fe0") // Road Runner
	{
		if (_mainPosition.y > BWAPI::Broodwar->mapHeight() * .80) // Bottom
			_naturalChoke = BWAPI::Position(BWAPI::Position(_natural->Location()).x, BWAPI::Position(_natural->Location()).y - 150);
		else if (_mainPosition.y < BWAPI::Broodwar->mapHeight() * .20) // top
			_naturalChoke = BWAPI::Position(BWAPI::Position(_natural->Location()).x + 100, BWAPI::Position(_natural->Location()).y + 200);
	}
	else if (BWAPI::Broodwar->mapHash() == "de2ada75fbc741cfa261ee467bf6416b10f9e301") // Python
	{
		if (_mainPosition.y < BWAPI::Broodwar->mapHeight() * .30) // Top
			_naturalChoke = BWAPI::Position(BWAPI::Position(_natural->Location()).x + 300, BWAPI::Position(_natural->Location()).y + 200);
		else if (_mainPosition.x < BWAPI::Broodwar->mapWidth() * .30) // left
			_naturalChoke = BWAPI::Position(BWAPI::Position(_natural->Location()).x + 300, BWAPI::Position(_natural->Location()).y + 200);
	}
	else if (BWAPI::Broodwar->mapHash() == "9bfc271360fa5bab3707a29e1326b84d0ff58911") // Tao Cross
	{
		if (_mainPosition.x < BWAPI::Broodwar->mapWidth() * .30) // left
			_naturalChoke = BWAPI::Position(BWAPI::Position(_natural->Location()).x + 300, BWAPI::Position(_natural->Location()).y + 100);
	}
	else if (BWAPI::Broodwar->mapHash() == "1e983eb6bcfa02ef7d75bd572cb59ad3aab49285") // Andromeda
	{
		if (_mainPosition.y > BWAPI::Broodwar->mapHeight() * .80 && _mainPosition.x > BWAPI::Broodwar->mapWidth() * .30) // Bottom Right
			_naturalChoke = BWAPI::Position(BWAPI::Position(_natural->Location()).x - 50, BWAPI::Position(_natural->Location()).y - 150);
	}


	shortest = 999999;
	distance = 0;
	if (_natural)
	{
		for (auto choke : mainToNat)
		{
			distance = abs((BWAPI::Position(choke->Center()).x - _natural->Center().x) - (BWAPI::Position(choke->Center()).y - _natural->Center().y));

			if (distance < shortest)
			{
				_mainChoke = BWAPI::Position(BWAPI::Position(choke->Center()).x, BWAPI::Position(choke->Center()).y);
				shortest = distance;
			}
		}
	}

	if (BWAPI::Broodwar->mapHash() == "c8386b87051f6773f6b2681b0e8318244aa086a6") // Neo Moon Glaive
	{
		// If our base is on the top of the map
		if (_mainPosition.y < BWAPI::Broodwar->mapHeight() / 2)
			_mainChoke += BWAPI::Position(200, 0);
	}

	// Small mineral tracking for removal, if there are any on the map
	for (auto mineral : BWAPI::Broodwar->getStaticMinerals())
	{
		if (mineral->getInitialResources() < 64)
		{
			length = -1;
			BWEM::CPPath temp = theMap.GetPath(BWAPI::Position(mineral->getPosition()), BWAPI::Position(_mainPosition), &length);
			
			if (length < 0)
			{
				_islandSmallMinerals.insert(mineral);
			}
			else
			{
				_smallMinerals.insert(mineral);
			}
		}
	}

	// _scanRotation is for scanning untaken bases to find hidden ones
	for (auto base : _otherBases)
	{
		_scanRotation.push_back(base.second->Location());
		_squadScoutRotation.push_back(base.second->Location());
	}

	// _neutralBuildings
	for (auto unit : BWAPI::Broodwar->getStaticNeutralUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Special_Power_Generator || 
			unit->getType() == BWAPI::UnitTypes::Special_Protoss_Temple ||
			unit->getType() == BWAPI::UnitTypes::Special_XelNaga_Temple)
		{
			_neutralBuildings.push_back(unit);
		}
	}

	// _enemyRace tracking so we have accurate decision making
	if (BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Random)
	{
		_enemyRace = BWAPI::Broodwar->enemy()->getRace();
	}
	else
		_enemyRace = BWAPI::Races::Unknown;


	// Add our initial bunker positions for the main and natural
	// BWAPI::Position((_infoManager.getEnemyNaturalPos().x * .70) + (BWAPI::Position(center).x * .30), (_infoManager.getEnemyNaturalPos().y * .70) + (BWAPI::Position(center).y * .40));
	//BWAPI::Position halfwayPoint = BWAPI::Position((BWAPI::Position(_mainPosition).x + _mainChoke.x) / 2, (BWAPI::Position(_mainPosition).y + _mainChoke.y) / 2);
	BWAPI::Position almostThere = BWAPI::Position(_mainChoke.x + (BWAPI::Position(_mainPosition).x - _mainChoke.x) * .25, _mainChoke.y + (BWAPI::Position(_mainPosition).y - _mainChoke.y) * .25);
	_mainBunkerPos = BWAPI::TilePosition(almostThere);

	if (BWAPI::Broodwar->mapHash() == "1e983eb6bcfa02ef7d75bd572cb59ad3aab49285") // Andromeda
	{
		if (_mainPosition.x < BWAPI::Broodwar->mapWidth() * .30) // left
			_mainBunkerPos = _mainBunkerPos - BWAPI::TilePosition(10, 0);
		else
			_mainBunkerPos = _mainBunkerPos + BWAPI::TilePosition(10, 0);
	}
	//_mainBunkerPos = BWAPI::TilePosition(getMainChokePos());

	if (BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(getNaturalChokePos())))
	{
		_NatBunkerPos = BWAPI::TilePosition(getNaturalChokePos());
	}
	else
	{
		_NatBunkerPos = BWAPI::Broodwar->getBuildLocation(BWAPI::UnitTypes::Terran_Bunker, BWAPI::TilePosition(getNaturalChokePos()));
	}
}

/****************************************************
* Delete things from our maps or lists
*****************************************************/
void InformationManager::checkForDeadList(std::list<BWAPI::Unit>& listToDeleteFrom)
{
	if (!listToDeleteFrom.size())
		return;

	for (std::list<BWAPI::Unit>::iterator unit = listToDeleteFrom.begin(); unit != listToDeleteFrom.end();)
	{
		if (!(*unit)->exists())
		{
			// Turrets need to be properly checked and removed from bases
			if ((*unit)->getType() == BWAPI::UnitTypes::Terran_Missile_Turret)
			{
				for (auto base : _ownedBases)
				{
					if (base.second->onTurretDestroy())
					{
						goto endOfChecks;
					}
				}

				for (auto base : _ownedIslandBases)
				{
					if (base.second->onTurretDestroy())
					{
						goto endOfChecks;
					}
				}

				for (auto base : _otherBases)
				{
					if (base.second->onTurretDestroy())
					{
						goto endOfChecks;
					}
				}

				for (auto base : _enemyBases)
				{
					if (base.second->onTurretDestroy())
					{
						goto endOfChecks;
					}
				}

				for (auto base : _islandBases)
				{
					if (base.second->onTurretDestroy())
					{
						goto endOfChecks;
					}
				}
			}

			endOfChecks:
			unit = listToDeleteFrom.erase(unit);
		}
		else
		{
			unit++;
		}
	}
}

void InformationManager::checkForDeadMap(std::map<BWAPI::Unit, int>& mapToDeleteFrom)
{
	if (!mapToDeleteFrom.size())
		return;

	for (std::map<BWAPI::Unit, int>::iterator unit = mapToDeleteFrom.begin(); unit != mapToDeleteFrom.end();)
	{
		if (!unit->first->exists())
		{
			unit = mapToDeleteFrom.erase(unit);
		}
		else
		{
			unit++;
		}
	}
}

/****************************************************
* Check our queue for a given unit to be made
*****************************************************/
void insanitybot::InformationManager::checkQueue(BWAPI::Unit myUnit)
{
	for (std::list<BWAPI::UnitType>::iterator queued = _queue.begin(); queued != _queue.end(); queued++)
	{
		if (*queued == myUnit->getType())
		{
			_queue.erase(queued);
			_reservedMinerals = _reservedMinerals - myUnit->getType().mineralPrice();
			_reservedGas = _reservedGas - myUnit->getType().gasPrice();
			return;
		}
	}
}

/****************************************************
* Check for dead command centers and handle them.
* This has a unique function as we also need to alter
* our base lists.
*****************************************************/
void InformationManager::checkForDeadCenters()
{
	for (std::list<BWAPI::Unit>::iterator center = _commandCenters.begin(); center != _commandCenters.end();)
	{
		if (!(*center) || !(*center)->exists())
		{
			for (std::map<BWAPI::Position, BWEM::Base *>::iterator & base = _ownedBases.begin(); base != _ownedBases.end(); base++)
			{
				//if (closeEnough((*center)->getPosition(), base->first))
				if (!base->second->getBaseCommandCenter() || !base->second->getBaseCommandCenter()->exists())
				{
					for (auto & worker : _workers)
					{
						if (!worker.first || !worker.first->exists())
							continue;

						if (worker.second == base->second)
						{
							if (!_ownedBases.empty())
							{
								worker.second = _ownedBases.begin()->second;
							}
							else
							{
								// ToDo: We're setting these workers to home just to put them somewhere
								// We have no bases left if _ownedBases is empty so the game is most likely lost
								worker.second = _home;
								worker.first->move(_home->Center());
							}
						}
					}

					// Attempted fix for queueing up a refinery on a dead base
					for (std::list<BWAPI::UnitType>::iterator it = _queue.begin(); it != _queue.end(); it++)
					{
						if (*it == BWAPI::UnitTypes::Terran_Refinery)
						{
							_queue.erase(it);
							if (_reservedMinerals >= 100)
							{
								_reservedMinerals -= 100;
							}
							break;
						}
					}

					base->second->setbaseCommandCenter(NULL);
					base->second->clearAssignmentList();
					_otherBases.insert(std::pair<BWAPI::Position, BWEM::Base *>(base->first, base->second));
					_ownedBases.erase(base->first);
					break;
				}
			}

			if (_ownedIslandBases.size())
			{
				for (std::map<BWAPI::Position, BWEM::Base *>::iterator & base = _ownedIslandBases.begin(); base != _ownedIslandBases.end(); base++)
				{
					if (closeEnough((*center)->getPosition(), base->first))
					{
						for (auto & worker : _islandWorkers)
						{
							if (!worker.first || !worker.first->exists())
								continue;

							if (worker.second == base->second)
							{
								if (!_ownedIslandBases.empty())
								{
									worker.second = _ownedIslandBases.begin()->second;
								}
								else
								{
									worker.second = _home;
									worker.first->move(_home->Center());
								}
							}
						}

						for (std::list<BWAPI::Unit>::iterator deadCenter = _islandCenters.begin(); deadCenter != _islandCenters.end(); deadCenter++)
						{
							if (*deadCenter == *center)
							{
								_islandCenters.erase(deadCenter);
								break;
							}
						}

						base->second->setbaseCommandCenter(NULL);
						base->second->clearAssignmentList();
						_islandBases.insert(std::pair<BWAPI::Position, BWEM::Base *>(base->first, base->second));
						_ownedIslandBases.erase(base->first);
						break;
					}
				}
			}


			center = _commandCenters.erase(center);

		}
		else
		{
			center++;
		}
	}
}

/****************************************************
* Check for dead refineries and handle them.
* This has a unique function as we also need to alter
* our base lists.
*****************************************************/
void InformationManager::checkForDeadRefineries()
{
	for (std::list<BWAPI::Unit>::iterator refinery = _refineries.begin(); refinery != _refineries.end();)
	{
		if (!(*refinery) || !(*refinery)->exists() || ((*refinery)->exists() && (*refinery)->getType() != BWAPI::UnitTypes::Terran_Refinery))
		{
			for (auto & base : _ownedBases)
			{
				if (base.second->Geysers().size() && abs(BWAPI::TilePosition((*refinery)->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition((*refinery)->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
				{
					_ownedBases[base.first]->setBaseRefinery(NULL);
					break;
				}
			}

			for (auto & base : _otherBases)
			{
				if (base.second->Geysers().size() && abs(BWAPI::TilePosition((*refinery)->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition((*refinery)->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
				{
					_otherBases[base.first]->setBaseRefinery(NULL);
					break;
				}
			}

			for (auto & base : _islandBases)
			{
				if (base.second->Geysers().size() && abs(BWAPI::TilePosition((*refinery)->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition((*refinery)->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
				{
					_islandBases[base.first]->setBaseRefinery(NULL);
					break;
				}
			}

			for (auto & base : _ownedIslandBases)
			{
				if (base.second->Geysers().size() && abs(BWAPI::TilePosition((*refinery)->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition((*refinery)->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
				{
					_ownedIslandBases[base.first]->setBaseRefinery(NULL);
					break;
				}
			}

			refinery = _refineries.erase(refinery);
		}
		else
		{
			refinery++;
		}
	}
}

/****************************************************
* Update is called each frame (close enough to each frame,
* see GameCommander), and will set our aggression level,
* initial scouting, and update our build order
*****************************************************/
void InformationManager::update()
{
	if (!getAggression() && isBio(_strategy) && _marines.size() > 30)
	{
		setAggression(true);
	}
	else if (!getAggression() && isMech(_strategy) && _tanks.size() >= 11)
	{
		setAggression(true);
	}
	else if (!getAggression() && isAllIn(_strategy) && _tanks.size() > 1)
	{
		setAggression(true);
	}

	if (_nukeDotDetected)
	{
		if (!BWAPI::Broodwar->getNukeDots().size())
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
	else
	{
		if (BWAPI::Broodwar->getNukeDots().size())
			_nukeDotDetected = true;
	}

	// Setup enemy Main Location

	if (_enemyNatPos == BWAPI::Position(0, 0) && _enemyBases.size())
	{
		for (auto startingBaseTile : BWAPI::Broodwar->getStartLocations())
		{
			for (auto base : _enemyBases)
			{
				if (closeEnough(base.first, BWAPI::Position(startingBaseTile)))
				{
					_enemyMainPos = startingBaseTile;
					break;
				}
			}
		}

		if (_enemyMainPos != BWAPI::TilePosition(0, 0) && _enemyBases.size() > 1 &&
			_enemyNatPos == BWAPI::Position(0, 0))
		{
			// Setup enemy natural location
			int shortest = 999999;
			int length = -1;
			for (auto base : _enemyBases)
			{
				if (base.second->baseHasGeyser())
				{
					BWEM::CPPath temp = theMap.GetPath(BWAPI::Position(_enemyMainPos), BWAPI::Position(base.first), &length);

					if (length < 1) continue;

					if (length < shortest)
					{
						shortest = length;
						_enemyNatPos = base.first;
					}
				}
			}
		}
		else if (_enemyNatPos == BWAPI::Position(0, 0) && _enemyMainPos != BWAPI::TilePosition(0,0))
		{
			// Setup enemy natural location
			int shortest = 999999;
			int length = -1;
			for (auto base : _otherBases)
			{
				if (base.second->baseHasGeyser())
				{
					BWEM::CPPath temp = theMap.GetPath(BWAPI::Position(_enemyMainPos), BWAPI::Position(base.first), &length);

					if (length < 0) continue;

					if (length < shortest)
					{
						shortest = length;
						_enemyNatPos = base.first;
					}
				}
			}
		}
	}
	else if (_enemyNatPos != BWAPI::Position(0, 0) && _enemyNatChoke == BWAPI::Position(0, 0))
	{
		int shortest = 999999;
		int distance = 0;
		int length = -1;
		for (auto base : _otherBases)
		{
			if (closeEnough(base.first, _enemyNatPos) || closeEnough(base.first, BWAPI::Position(_enemyMainPos)))
				continue;

			BWEM::CPPath enemyNatToSomewhere = theMap.GetPath(BWAPI::Position(base.first), _enemyNatPos, &length);

			if (length < 0) continue;

			for (auto choke : enemyNatToSomewhere)
			{
				distance = abs((BWAPI::Position(choke->Center()).x - _enemyNatPos.x) - (BWAPI::Position(choke->Center()).y - _enemyNatPos.y));

				if (distance < shortest)
				{
					_enemyNatChoke = BWAPI::Position(BWAPI::Position(choke->Center()).x, BWAPI::Position(choke->Center()).y);
					shortest = distance;
				}
			}

			break;
		}
	}

	//Initial Scouting
	if (_enemyBases.size() < 1 && _workers.size() >= 7 && (!_scout || !_scout->exists()) && BWAPI::Broodwar->getFrameCount() < 20000 && !_enemyRushing)
	{
		for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
		{
			if (!it->first || !it->first->exists())
				continue;

			if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
			{
				BWAPI::Broodwar << it->first->getType() << " found in worker list." << std::endl;
				continue;
			}

			if (it->first->getType().isWorker() && (it->first->isIdle() || it->first->isGatheringMinerals())
				&& !it->first->isCarryingGas() && !it->second->isGasWorker(it->first) && !it->first->isConstructing())
			{
				_scout = it->first;

				for (auto & base : _ownedBases)
				{
					if (base.second == it->second)
					{
						base.second->removeAssignment(it->first);
						break;
					}
				}
				
				_workers.erase(it);
				break;
			}
		}
	}
	else if (_enemyBases.size() && _scout)
	{
		if (_scout->exists())
		{
			_workers.insert(std::make_pair(_scout, _ownedBases.begin()->second));
		}

		_scout = NULL;
	}

	if (BWAPI::Broodwar->getFrameCount() < 5000)
	{
		_enemyRushing = checkForEnemyRush();
	}

	if (_enemyRushing && isTwoBasePlay(_strategy))
	{
		if (_enemyRace == BWAPI::Races::Zerg)
		{
			for (auto item : _queue)
			{
				_reservedMinerals -= item.mineralPrice();
				_reservedGas -= item.gasPrice();
			}

			_queue.clear();
			_strategy = "8RaxDef";
		}
		else
		{
			for (auto item : _queue)
			{
				_reservedMinerals -= item.mineralPrice();
				_reservedGas -= item.gasPrice();
			}

			_queue.clear();
			_strategy = "1BaseMech";
		}
	}

	//Update build order
	updateBuildOrder();

	//Clear the lists of units
	_construction.clear();
	_injuredBuildings.clear();
	_floatingBuildings.clear();

	std::vector<std::reference_wrapper<std::map<BWAPI::Unit, int>>> masterUnitList = { _marines, _medics, _ghosts, _vultures, _goliaths, _tanks, _vessels, _dropships };
	std::vector<std::reference_wrapper<std::list<BWAPI::Unit>>> masterBuildingList = { _barracks, _factories, _starports, _science, _armories, _academy, _engibays, _turrets, _bunkers, _comsats };
	
	for (auto &unitList : masterUnitList) checkForDeadMap(unitList);
	for (auto &buildingList : masterBuildingList) checkForDeadList(buildingList);

	// Command centers being lost requires a more unique solution
	// We'll check that here for now
	if (_commandCenters.size())
	{
		checkForDeadCenters();
	}
	// Refineries as well
	if (_refineries.size())
	{
		checkForDeadRefineries();
	}
	// Workers
	if (_workers.size())
	{
		for (std::map<BWAPI::Unit, BWEM::Base *>::iterator worker = _workers.begin(); worker != _workers.end();)
		{
			if (!worker->first || !worker->first->exists())
			{
				for (auto & base : _ownedBases)
				{
					base.second->onUnitDestroy(worker->first);
				}
				for (auto & islandBase : _ownedIslandBases)
				{
					islandBase.second->onUnitDestroy(worker->first);
				}

				worker = _workers.erase(worker);
			}
			else
			{
				worker++;
			}
		}
	}

	//Loop through our units and add constructing buildings to our list
	for (BWAPI::Unit myUnit : _self->getUnits())
	{
		if (!myUnit || !myUnit->exists())
			continue;

		if (myUnit->getType().isBuilding() && (myUnit->isBeingConstructed() || myUnit->canCancelConstruction()))
		{
			_construction.push_back(myUnit);
		}
		else if (myUnit->getType().isBuilding() && myUnit->isCompleted() &&
			myUnit->getHitPoints() < myUnit->getType().maxHitPoints() && !myUnit->isFlying())
		{
			_injuredBuildings.push_back(myUnit);
		}

		else if (myUnit->getType().isBuilding() && myUnit->isFlying())
		{
			_floatingBuildings.push_back(myUnit);
		}

		// Insert Workers into our list if they're not taken elsewhere
		if (myUnit->getType().isWorker() && myUnit->getPlayer() == _self)
		{
			bool gotADegree = false;
			for (auto worker : _repairWorkers)
			{
				if (worker == myUnit)
				{
					gotADegree = true;
					break;
				}
			}

			for (auto worker : _bullyHunters)
			{
				if (worker == myUnit)
				{
					gotADegree = true;
					break;
				}
			}

			for (auto worker : _fieldEngineers)
			{
				if (worker == myUnit)
				{
					gotADegree = true;
					break;
				}
			}

			if (_workers.find(myUnit) != _workers.end() ||
				gotADegree || myUnit == _scout)
				continue;
			

			for (auto & base : _ownedBases)
			{
				if (closeEnough(myUnit->getPosition(), base.first))
				{
					//_workers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(myUnit, base.second));
					_workers.insert(std::make_pair(myUnit, base.second));
					break;
				}
			}

			if (_ownedIslandBases.size())
			{
				int closest = 9999999;
				int length = -1;
				theMap.GetPath(myUnit->getPosition(), BWAPI::Position(_mainPosition), &length);
				BWEM::Base * island = _ownedIslandBases.begin()->second;

				for (auto & base : _ownedIslandBases)
				{
					if (closeEnough(myUnit->getPosition(), base.first))
					{
						_islandWorkers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(myUnit, base.second));
						goto justBuilt;
					}

					if (length < 0 && myUnit->getDistance(base.first) < closest)
					{
						island = base.second;
					}
				}

				if (closest < 9999999)
				{
					_islandWorkers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(myUnit, island));
				}
			}

		justBuilt:
			// If they're not immediately next to a command center, send them to the closest base in our list
			if (_workers.find(myUnit) == _workers.end() && _islandWorkers.find(myUnit) == _islandWorkers.end()
				&& _ownedBases.size())
			{
				int closestBase = myUnit->getDistance(_ownedBases.begin()->first);
				BWEM::Base * assignedBase = _ownedBases.begin()->second;

				for (auto base : _ownedBases)
				{
					if (myUnit->getDistance(_ownedBases.begin()->first) < closestBase)
					{
						closestBase = myUnit->getDistance(_ownedBases.begin()->first);
						assignedBase = base.second;
					}
				}

				_workers.insert(std::make_pair(myUnit, assignedBase));
			}
		}
		// Insert Marines
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Marine && myUnit->getPlayer() == _self)
		{
			if (_marines.find(myUnit) != _marines.end())
				continue;

			_marines.insert(std::pair<BWAPI::Unit, int>(myUnit, 0));
		}
		// Medics
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Medic && myUnit->getPlayer() == _self)
		{
			if (_medics.find(myUnit) != _medics.end())
				continue;

			_medics.insert(std::pair<BWAPI::Unit, int>(myUnit, 0));
		}
		// Ghosts
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Ghost && myUnit->getPlayer() == _self)
		{
			if (_ghosts.find(myUnit) != _ghosts.end())
				continue;

			_ghosts.insert(std::pair<BWAPI::Unit, int>(myUnit, 0));
		}
		// Dropships
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Dropship && myUnit->getPlayer() == _self)
		{
			if (_dropships.find(myUnit) != _dropships.end())
				continue;

			_dropships.insert(std::pair<BWAPI::Unit, int>(myUnit, 0));
		}
		// Vessels
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Science_Vessel && myUnit->getPlayer() == _self)
		{
			if (_vessels.find(myUnit) != _vessels.end())
				continue;

			_vessels.insert(std::pair<BWAPI::Unit, int>(myUnit, 0));
		}
		// Vultuers
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Vulture && myUnit->getPlayer() == _self)
		{
			if (_vultures.find(myUnit) != _vultures.end())
				continue;

			_vultures.insert(std::pair<BWAPI::Unit, int>(myUnit, 0));
		}
		// Tanks
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode && myUnit->getPlayer() == _self)
		{
			if (_tanks.find(myUnit) != _tanks.end())
				continue;

			_tanks.insert(std::pair<BWAPI::Unit, int>(myUnit, 0));
		}
		// Goliaths
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Goliath && myUnit->getPlayer() == _self)
		{
			if (_goliaths.find(myUnit) != _goliaths.end())
				continue;

			_goliaths.insert(std::pair<BWAPI::Unit, int>(myUnit, 0));
		}
		// Barracks
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Barracks && myUnit->getPlayer() == _self)
		{
			if (std::find(_barracks.begin(), _barracks.end(), myUnit) != _barracks.end())
				continue;

			_barracks.push_back(myUnit);
			checkQueue(myUnit);
		}
		// Factory
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Factory && myUnit->getPlayer() == _self)
		{
			if (std::find(_factories.begin(), _factories.end(), myUnit) != _factories.end())
				continue;

			_factories.push_back(myUnit);
			checkQueue(myUnit);
		}
		// Starport
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Starport && myUnit->getPlayer() == _self)
		{
			if (std::find(_starports.begin(), _starports.end(), myUnit) != _starports.end())
				continue;

			_starports.push_back(myUnit);
			checkQueue(myUnit);
		}
		// Academy
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Academy && myUnit->getPlayer() == _self)
		{
			if (std::find(_academy.begin(), _academy.end(), myUnit) != _academy.end())
				continue;

			_academy.push_back(myUnit);
			checkQueue(myUnit);
		}
		// Engibays
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Engineering_Bay && myUnit->getPlayer() == _self)
		{
			if (std::find(_engibays.begin(), _engibays.end(), myUnit) != _engibays.end())
				continue;

			_engibays.push_back(myUnit);
			checkQueue(myUnit);
		}
		// Armory
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Armory && myUnit->getPlayer() == _self)
		{
			if (std::find(_armories.begin(), _armories.end(), myUnit) != _armories.end())
				continue;

			_armories.push_back(myUnit);
			checkQueue(myUnit);
		}
		// Science Facility
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Science_Facility && myUnit->getPlayer() == _self)
		{
			if (std::find(_science.begin(), _science.end(), myUnit) != _science.end())
				continue;

			_science.push_back(myUnit);
			checkQueue(myUnit);
		}
		// Addons
		else if (myUnit->getType().isAddon() && myUnit->getPlayer() == _self)
		{
			if (myUnit->getType() == BWAPI::UnitTypes::Terran_Comsat_Station)
			{
				if (std::find(_comsats.begin(), _comsats.end(), myUnit) != _comsats.end())
					continue;

				_comsats.push_back(myUnit);
			}
			else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Machine_Shop)
			{
				if (std::find(_machineShops.begin(), _machineShops.end(), myUnit) != _machineShops.end())
					continue;

				_machineShops.push_back(myUnit);
			}
			else
			{
				if (std::find(_addons.begin(), _addons.end(), myUnit) != _addons.end())
					continue;

				_addons.push_back(myUnit);
			}
		}
		// Turrets
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Missile_Turret && myUnit->getPlayer() == _self)
		{
			if (std::find(_turrets.begin(), _turrets.end(), myUnit) != _turrets.end())
				continue;

			for (auto & base : _ownedBases)
			{
				if (myUnit->getDistance(base.first) < 400)
				{
					base.second->addTurrets(myUnit);
					break;
				}
			}

			for (auto & base : _ownedIslandBases)
			{
				if (myUnit->getDistance(base.first) < 400)
				{
					base.second->addTurrets(myUnit);
					break;
				}
			}

			_turrets.push_back(myUnit);
			checkQueue(myUnit);
		}
		// Insert command center as new base being taken
		else if (myUnit->getType().isResourceDepot() && myUnit->getPlayer() == _self)
		{
			if (std::find(_commandCenters.begin(), _commandCenters.end(), myUnit) != _commandCenters.end())
				continue;

			for (std::map<BWAPI::Position, BWEM::Base *>::iterator & base = _otherBases.begin(); base != _otherBases.end(); base++)
			{
				if (closeEnough(myUnit->getPosition(), base->first))
				{
					base->second->setbaseCommandCenter(myUnit);
					_ownedBases.insert(std::pair<BWAPI::Position, BWEM::Base *>(base->first, base->second));
					_otherBases.erase(base);
					break;
				}
			}

			bool isIslandCenter = false;

			for (std::map<BWAPI::Position, BWEM::Base *>::iterator & base = _islandBases.begin(); base != _islandBases.end(); base++)
			{
				if (closeEnough(myUnit->getPosition(), base->first))
				{
					if (_islandBuilder && _islandBuilder->exists())
					{
						_islandWorkers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(_islandBuilder, base->second));
						_islandBuilder = NULL;
					}

					_islandCenters.push_back(myUnit);
					isIslandCenter = true;
					base->second->setbaseCommandCenter(myUnit);
					_ownedIslandBases.insert(std::pair<BWAPI::Position, BWEM::Base *>(base->first, base->second));
					_islandBases.erase(base);
					break;
				}
			}

			if (!isIslandCenter)
				_commandCenters.push_back(myUnit);

			checkQueue(myUnit);
		}
		// Refineries should be assigned to their respective bases
		else if (myUnit->getType().isRefinery() && myUnit->getPlayer() == _self)
		{
			if (std::find(_refineries.begin(), _refineries.end(), myUnit) != _refineries.end())
				continue;

			for (auto base : _ownedBases)
			{
				if (base.second->baseHasGeyser() && abs(BWAPI::TilePosition(myUnit->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition(myUnit->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
				{
					_ownedBases[base.first]->setBaseRefinery(myUnit);
					break;
				}
			}

			for (auto base : _ownedIslandBases)
			{
				if (base.second->baseHasGeyser() && abs(BWAPI::TilePosition(myUnit->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition(myUnit->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
				{
					_ownedIslandBases[base.first]->setBaseRefinery(myUnit);
					break;
				}
			}

			_refineries.push_back(myUnit);
			checkQueue(myUnit);
		}
		// Bunkers are tracked seperately for repair purposes
		else if (myUnit->getType() == BWAPI::UnitTypes::Terran_Bunker && myUnit->getPlayer() == _self)
		{
			if (std::find(_bunkers.begin(), _bunkers.end(), myUnit) != _bunkers.end())
				continue;

			_bunkers.push_back(myUnit);
			checkQueue(myUnit);
		}
	}

	if (BWAPI::Broodwar->getFrameCount() % 10 == 0)
	{
		for (auto & base : _ownedBases)
		{
			base.second->cleanUpZombieTasks();
		}
	}

	for (auto structure : _enemyStructurePositions)
	{
		BWAPI::TilePosition structureTilePosition = BWAPI::TilePosition(structure.x / 32, structure.y / 32);

		if (BWAPI::Broodwar->isVisible(structureTilePosition))
		{
			bool stillThere = false;

			for (auto u : BWAPI::Broodwar->enemy()->getUnits()) 
			{
				if (!u || !u->exists())
					continue;

				if ((u->getType().isBuilding()) && (u->getPosition() == structure)) 
				{
					stillThere = true;
					break;
				}
			}

			// if there is no building, remove that from our list
			if (stillThere == false) 
			{
				_enemyStructurePositions.remove(structure);
				break;
			}
		}
	}


	// Attempted fix for canceled zerg structures not clearing tracked enemy bases
	for (auto & base : _enemyBases)
	{
		if (BWAPI::Broodwar->isVisible(base.second->Location()))
		{
			bool exists = false;
			for (auto unit : _enemy->getUnits())
			{
				if (unit && unit->exists())
				{
					if (unit->getType().isResourceDepot() && closeEnough(unit->getPosition(), base.first))
					{
						exists = true;
					}
				}
			}

			if (!exists)
			{
				base.second->setbaseCommandCenter(NULL);
				base.second->clearAssignmentList();
				_otherBases.insert(std::pair<BWAPI::Position, BWEM::Base *>(base.first, base.second));
				_enemyBases.erase(base.first);

				if (_targetDefended)
					_targetDefended = false;

				break;
			}

		}
	}

	// Scans
	if (_comsats.size())
	{
		for (auto unit : _enemy->getUnits())
		{
			if (!unit)
				continue;

			if (unit->isVisible() && !unit->isDetected() && unit->getType() != BWAPI::UnitTypes::Protoss_Observer)
			{
				for (auto com : _comsats)
				{
					if (!com || !com->exists()) continue;

					if (com->getEnergy() >= BWAPI::TechTypes::Scanner_Sweep.energyCost() && com->getPlayer() == _self &&
						BWAPI::Broodwar->getFrameCount() - _lastScanFrame > 250)
					{
						_lastScanFrame = BWAPI::Broodwar->getFrameCount();
						com->useTech(BWAPI::TechTypes::Scanner_Sweep, unit->getPosition());
					}
				}
			}
		}

		for (auto com : _comsats)
		{
			if (!com || !com->exists()) continue;

			if (com->getEnergy() >= 195 && com->getPlayer() == _self)
			{
				if (!BWAPI::Broodwar->isVisible(_enemyMainPos) && BWAPI::Broodwar->getFrameCount() - _lastScanFrame > 1500)
				{
					_lastScanFrame = BWAPI::Broodwar->getFrameCount();
					com->useTech(BWAPI::TechTypes::Scanner_Sweep, BWAPI::Position(_enemyMainPos));
					continue;
				}
				BWAPI::TilePosition nextUp = _scanRotation.front();
				if (!BWAPI::Broodwar->isVisible(nextUp.x, nextUp.y))
				{
					com->useTech(BWAPI::TechTypes::Scanner_Sweep, BWAPI::Position(nextUp));
				}

				_scanRotation.push_back(_scanRotation.front());
				_scanRotation.erase(_scanRotation.begin());
			}
		}
	}
}

/****************************************************
* UpdateBuildOrder will tell our creationManager what
* we should build, set our reserved resources, and
* determine if we should expand at the moment.
*****************************************************/
void insanitybot::InformationManager::updateBuildOrder()
{
	int numEngiBaysFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Engineering_Bay);

	/******************************************************
	 * Important section: Very primative build order. This
	 * is where we will decide what needs to be built and
	 * minerals/gas will be reserved for it.
	 ******************************************************/
	if (_queue.empty())
	{
		if (_strategy == "SKTerran")
		{
			SKTerran();
		}
		else if (_strategy == "Nuke")
		{
			Nuke();
		}
		else if (_strategy == "Mech")
		{
			Mech();
		}
		else if (_strategy == "8RaxDef")
		{
			EightRaxDef();
		}
		else if (_strategy == "1BaseMech")
		{
			OneBaseMech();
		}
		else if (_strategy == "OneFacAllIn")
		{
			OneFacAllIn();
		}
		else if (_strategy == "MechAllIn")
		{
			MechAllIn();
		}

		int extraCoverInMain = 0;

		if (_enemyRace == BWAPI::Races::Zerg && _enemyHasAir)
			extraCoverInMain = 3;

		if (numEngiBaysFinished && 
			getNumTotalUnit(BWAPI::UnitTypes::Terran_Missile_Turret) < (getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center) - _ownedIslandBases.size()) * 3 + extraCoverInMain &&
			(_enemyRace == BWAPI::Races::Zerg || _enemyHasAir))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Missile_Turret);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Missile_Turret.mineralPrice());
		}
	}
}
	/**************************************************
	 * a very common pattern in TvZ is this:
	 * 1 barracks
	 * expand
	 * optional 2nd barracks
	 * academy and ebay (then stim and +1 weapons)
	 * 3 barracks
	 * factory (unused but needed for starport)
	 * 5 barracks
	 * starport
	 * science facility
	 * 2nd starport
	 * 7 barracks
	 * produce marine+medic+vessel off 7 barracks+2 port until main is about to run out
	 * or in TvP, going mech, on two bases:
	 * - macro play would be 3 fac + upgrades + 3rd base
	 * - pressure into transition would be 4 fac
	 * - all-in aggression would be 5 or 6 fac
	 * TvT looks similar but i'm less familiar
	 * 5 fac still being very aggressive
	 * if memes = nuke you can absolutely do that with the pattern i outlined above -- just swap the 2nd starport for nuke silo
	***************************************************/

/**************************************************
 * Fast expand, M-M + Vessel into aggressive play
***************************************************/
void insanitybot::InformationManager::SKTerran()
{
	int numRaxFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);
	int numFactoryFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Factory);
	int numStarportFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Starport);
	int numEngiBaysFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Engineering_Bay);
	bool hasAcademy = _academy.size();
	bool hasScience = _science.size();

	if (!_initialBarracks)
	{
		if (_self->minerals() > 100 && getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_initialBarracks = true;
		}
	}
	else
	{
		// 2nd barracks
		if (numRaxFinished <= 1 && (_ownedBases.size() >= 2 || _self->deadUnitCount(BWAPI::UnitTypes::Terran_Command_Center) > 1) && _barracks.size() <= 1
			&& _marines.size() > 2)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		// 3rd rax, academy, and engibay
		if (numRaxFinished == 2 && _barracks.size() == 2 && (_marines.size() > 8 || _self->minerals() > 300))
		{
			if (!hasAcademy)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
			}
			if (getNumTotalUnit(BWAPI::UnitTypes::Terran_Engineering_Bay) < 1)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
			}

			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		// fact for port
		if (hasAcademy && _factories.size() < 1 && _refineries.size() &&
			(_marines.size() > 16 || _self->minerals() > 300))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}

		// 5 rax and port
		if (numFactoryFinished >= 1 && (_marines.size() > 24 || _self->minerals() > 300) && _starports.size() == 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}

		// science facility
		if (numStarportFinished >= 1 && !hasScience && getNumTotalUnit(BWAPI::UnitTypes::Terran_Science_Facility) < 1)
		{
			if (getNumTotalUnit(BWAPI::UnitTypes::Terran_Engineering_Bay) < 2)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
			}
			_queue.push_back(BWAPI::UnitTypes::Terran_Science_Facility);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Science_Facility.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Science_Facility.gasPrice());
			// 2nd port
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}
		else if (hasScience && _barracks.size() < 7)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		// 1st expansion
		if (getNumTotalUnit(BWAPI::UnitTypes::Terran_Marine) > 0 && _ownedBases.size() < 2)
		{
			if (_bunkers.size() && !closeEnough((*_bunkers.begin())->getPosition(), BWAPI::Position(_NatBunkerPos)))
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
			}
			else if (!_bunkers.size())
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
			}
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}

		// Island expansions
		if (_ownedBases.size() >= 2 && shouldExpand() && getNumFinishedUnit(BWAPI::UnitTypes::Terran_Dropship) && _islandBases.size() &&
			!_islandExpand && _islandBuilder == NULL && !_ownedIslandBases.size())
		{
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
			_islandExpand = true;
		}
		// all other expansions
		else if (_ownedBases.size() >= 2 && shouldExpand() && getReservedMinerals() < 400)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}
	}

	if (hasAcademy && getNumTotalUnit(BWAPI::UnitTypes::Terran_Refinery) < numGeyserBases())
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
	}
}

/**************************************************
 * Fast expand, SKTerran with nukes and ghosts
***************************************************/
void insanitybot::InformationManager::Nuke()
{
	int numRaxFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);
	int numFactoryFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Factory);
	int numStarportFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Starport);
	int numEngiBaysFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Engineering_Bay);
	bool hasAcademy = _academy.size();
	bool hasScience = _science.size();

	if (!_initialBarracks)
	{
		if (_self->minerals() > 100 && getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_initialBarracks = true;
		}
	}
	else
	{
		// 2nd barracks
		if (numRaxFinished <= 1 && (_ownedBases.size() >= 2 || _self->deadUnitCount(BWAPI::UnitTypes::Terran_Command_Center) > 1) && _barracks.size() <= 1
			&& _marines.size() > 2)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		// 3rd rax, academy, and engibay
		if (numRaxFinished == 2 && _barracks.size() == 2 && (_marines.size() > 8 || _self->minerals() > 300))
		{
			if (!hasAcademy)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
			}
			if (getNumTotalUnit(BWAPI::UnitTypes::Terran_Engineering_Bay) < 1)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
			}

			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		// fact for port
		if (hasAcademy && _factories.size() < 1 && _refineries.size() &&
			(_marines.size() > 16 || _self->minerals() > 300))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}

		// 5 rax and port
		if (numFactoryFinished >= 1 && (_marines.size() > 24 || _self->minerals() > 300) && _starports.size() == 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}

		// science facility
		if (numStarportFinished >= 1 && !hasScience && getNumTotalUnit(BWAPI::UnitTypes::Terran_Science_Facility) < 1)
		{
			if (getNumTotalUnit(BWAPI::UnitTypes::Terran_Engineering_Bay) < 2)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
			}
			_queue.push_back(BWAPI::UnitTypes::Terran_Science_Facility);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Science_Facility.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Science_Facility.gasPrice());
		}
		else if (hasScience && _barracks.size() < 7)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		if (getNumFinishedUnit(BWAPI::UnitTypes::Terran_Nuclear_Missile) && numFactoryFinished >= 1 && _starports.size() <= 1)
		{
			// 2nd port
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}

		// 1st expansion
		if (getNumTotalUnit(BWAPI::UnitTypes::Terran_Marine) > 0 && _ownedBases.size() < 2)
		{
			if (_bunkers.size() && !closeEnough((*_bunkers.begin())->getPosition(), BWAPI::Position(_NatBunkerPos)))
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
			}
			else if (!_bunkers.size())
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
			}
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}

		// Island expansions
		if (_ownedBases.size() >= 2 && shouldExpand() && getNumFinishedUnit(BWAPI::UnitTypes::Terran_Dropship) && _islandBases.size() &&
			!_islandExpand && _islandBuilder == NULL && !_ownedIslandBases.size())
		{
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
			_islandExpand = true;
		}
		// all other expansions
		else if (_ownedBases.size() >= 2 && shouldExpand() && getReservedMinerals() < 400)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}
	}

	bool trainingNuke = false;

	for (auto addon : _addons)
	{
		if (addon->getType() == BWAPI::UnitTypes::Terran_Nuclear_Silo && !addon->isIdle())
		{
			trainingNuke = true;
			break;
		}
	}

	if (getNumFinishedUnit(BWAPI::UnitTypes::Terran_Nuclear_Silo) && getNumTotalUnit(BWAPI::UnitTypes::Terran_Nuclear_Missile) < 1 && !trainingNuke)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Nuclear_Missile);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Nuclear_Missile.mineralPrice());
		setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Nuclear_Missile.gasPrice());
	}

	if (hasAcademy && getNumTotalUnit(BWAPI::UnitTypes::Terran_Refinery) < numGeyserBases())
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
	}

	if (!hasAcademy && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Academy) > 0)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
	}
}

/**************************************************
 * Fast Expand into Mech
***************************************************/
void insanitybot::InformationManager::Mech()
{
	int numRaxFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);
	int numFactoryFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Factory);
	int numStarportFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Starport);
	int numEngiBaysFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Engineering_Bay);
	bool hasAcademy = _academy.size();
	bool hasScience = _science.size();

	if (!_initialBarracks)
	{
		if (_self->minerals() > 100 && getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
			_initialBarracks = true;
		}
	}
	else
	{
		if (_barracks.size() == 0 && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Barracks) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}

		if (_engibays.size() == 0 && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Engineering_Bay) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
		}

		// first factory
		if (_self->gas() >= 100 && _factories.size() == 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}
		// second factory, engibay, academy
		else if (numFactoryFinished == 1 && _factories.size() == 1 && _machineShops.size() && (_self->isResearching(BWAPI::TechTypes::Tank_Siege_Mode) || _self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode)))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
			if (_engibays.size() < 1)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
			}

			if (!hasAcademy)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
			}
		}
		// third factory and armory
		else if (numFactoryFinished == 2 && _factories.size() == 2 && (_tanks.size() > 3 || _self->minerals() > 300))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
			if (_armories.size() < 2)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Armory);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Armory.mineralPrice());
				setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Armory.gasPrice());
			}
		}
		// forth and fifth factory
		else if (numFactoryFinished < 5 && _factories.size() < 5 && _tanks.size() > 5)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
			if (_armories.size() < 2)
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Armory);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Armory.mineralPrice());
				setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Armory.gasPrice());
			}
		}

		// Starport
		if (numFactoryFinished >= 3 && _starports.size() < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}

		// science facility
		if (numStarportFinished >= 1 && !hasScience && getNumTotalUnit(BWAPI::UnitTypes::Terran_Science_Facility) < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Science_Facility);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Science_Facility.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Science_Facility.gasPrice());
		}


		// 1st expansion
		if ((getNumTotalUnit(BWAPI::UnitTypes::Terran_Marine) > 0 || _self->deadUnitCount(BWAPI::UnitTypes::Terran_Marine) == 4) && _ownedBases.size() < 2 && !shouldHaveDefenseSquad(false))
		{
			if (_self->deadUnitCount(BWAPI::UnitTypes::Terran_Bunker) < 1 && !_bunkers.size())
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
			}


			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}
		// Island expansions
		else if (_ownedBases.size() >= 2 && shouldExpand() && getNumFinishedUnit(BWAPI::UnitTypes::Terran_Dropship) && _islandBases.size() &&
			!_islandExpand && _islandBuilder == NULL && !_ownedIslandBases.size())
		{
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
			_islandExpand = true;
		} // Other Expansions
		else if (_ownedBases.size() >= 2 && shouldExpand() && (_self->minerals() > 800 || BWAPI::Broodwar->getFrameCount() > 20000) &&
			!_islandExpand)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}

		// Factory up to 7
		if (_factories.size() < 7 && numFactoryFinished >= 5 && _ownedBases.size() > 2 && _self->minerals() > 300 && _self->gas() > 150)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}

		if (!hasAcademy && _self->deadUnitCount(BWAPI::UnitTypes::Terran_Academy) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
		}

		// Geysers
		if (numRaxFinished && getNumTotalUnit(BWAPI::UnitTypes::Terran_Refinery) < numGeyserBases() &&
			getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center) >= numGeyserBases())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
		}
	}
}

/**************************************************
 * Anti-Rush Builds
***************************************************/
void insanitybot::InformationManager::EightRaxDef()
{
	int numRaxFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);

	if (_marines.size() > 18)
	{
		if (_enemyRace == BWAPI::Races::Zerg)
			_strategy = "Nuke";
		else
			_strategy = "Mech";

		_enemyRushing = false;
		return;
	}

	if (!_initialBarracks || _barracks.size() < 1)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		_initialBarracks = true;
	}
	else if (numRaxFinished && _bunkers.size() < 1)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
	}
	else if (_bunkers.size() && _barracks.size() < 2 && (_marines.size() > 3 || _self->minerals() > 300))
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
	}

	if (numRaxFinished >= 2 && !getRefineries().size())
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
	}
	else if (numRaxFinished && getRefineries().size() && !getAcademy().size())
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
	}

	if (numRaxFinished >= 2 && !getEngibays().size() && _self->minerals() > 200)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
	}
}

// Defensive Mech
void insanitybot::InformationManager::OneBaseMech()
{
	int numRaxFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);

	if (_tanks.size() > 2)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());

		if (_enemyRace == BWAPI::Races::Zerg)
			_strategy = "SKTerran";
		else
			_strategy = "Mech";

		_enemyRushing = false;
		return;
	}

	if (!_initialBarracks || _barracks.size() < 1)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		_initialBarracks = true;
	}
	else if (numRaxFinished && _bunkers.size() + _self->deadUnitCount(BWAPI::UnitTypes::Terran_Bunker) < 1)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
	}
	else if ((_bunkers.size() || _self->deadUnitCount(BWAPI::UnitTypes::Terran_Bunker) > 0) && _barracks.size() && _refineries.size() < 1)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
	}
	else if (_barracks.size() && _self->gas() > 100 && _factories.size() < 2)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
		setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
	}

	if (_tanks.size() && !_engibays.size())
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
	}
}

/**********************************************
* One base all in
***********************************************/
// All in Bio Mech
void insanitybot::InformationManager::OneFacAllIn()
{
	bool needDetection = false;

	for (auto unit : _enemyUnits)
	{
		if (unit && unit->exists())
		{
			if (unit->isCloaked() || unit->isBurrowed())
			{
				needDetection = true;
				break;
			}
		}
	}

	int numRaxFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);

	if (!_initialBarracks && _workers.size() >= 8 && getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		_initialBarracks = true;
	}
	else
	{
		if (_marines.size() && _bunkers.size() + _self->deadUnitCount(BWAPI::UnitTypes::Terran_Bunker) < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
		}

		if (_barracks.size() && _refineries.size() < 1 && getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
		}
		else if (_barracks.size() < 2 && _refineries.size() && _workers.size() > 15 && getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0 &&
			(_self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode) || _self->isResearching(BWAPI::TechTypes::Tank_Siege_Mode)))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}
		else if (_barracks.size() < 3 &&  _self->minerals() > 300 && _self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		}
		else if (((_barracks.size() <= 3 && _self->minerals() > 300 && _self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode)) ||
			_enemyHasAir) && !_engibays.size())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
		}

		if (_barracks.size() && _self->gas() > 100 && _factories.size() < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}

		if (_barracks.size() && _factories.size() && _academy.size() < 1 && _marines.size() > 5 && 
			_tanks.size() && (_self->isResearching(BWAPI::TechTypes::Tank_Siege_Mode) || _self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode)))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
		}

		if (_factories.size() && _self->minerals() > 500 && _self->gas() > 300 && _armories.size() < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Armory);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Armory.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Armory.gasPrice());
		}

		// Starport for detection
		if (((_factories.size() && _tanks.size() > 6) || needDetection) && !_starports.size())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Starport);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Starport.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Starport.gasPrice());
		}

		if (getNumFinishedUnit(BWAPI::UnitTypes::Terran_Starport) && !_science.size())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Science_Facility);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Science_Facility.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Science_Facility.gasPrice());
		}

		if (shouldExpand())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}

		// Geysers
		if (numRaxFinished && getNumTotalUnit(BWAPI::UnitTypes::Terran_Refinery) < numGeyserBases() &&
			getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center) >= numGeyserBases() && 
			_ownedBases.size() > 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
		}
	}
}

// Limited bio mech all in
void insanitybot::InformationManager::MechAllIn()
{
	int numRaxFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);

	if (!_initialBarracks && _workers.size() >= 8 && getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
	{
		_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
		setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
		_initialBarracks = true;
	}
	else
	{
		if (_barracks.size() && _refineries.size() < 1 && getNumTotalUnit(BWAPI::UnitTypes::Terran_Supply_Depot) > 0)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
		}
		else if (((_barracks.size() && _self->minerals() > 300 && _self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode)) ||
			_enemyHasAir) && !_engibays.size())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Engineering_Bay);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Engineering_Bay.mineralPrice());
		}

		if (_barracks.size() && _self->gas() > 100 && _factories.size() < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}
		else if (_barracks.size() && _factories.size() < 2 && _factories.size() == 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}
		/*else if (_barracks.size() && _factories.size() < 3 && _factories.size() == 2 &&
			_self->minerals() > 300 && _self->gas() > 150)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
		}*/

		if (_barracks.size() && _factories.size() && _academy.size() < 1 && _tanks.size() > 4 &&
			(_self->isResearching(BWAPI::TechTypes::Tank_Siege_Mode) || _self->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode)))
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Academy);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Academy.mineralPrice());
		}

		if (_factories.size() && _self->minerals() > 500 && _self->gas() > 300 && _armories.size() < 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Armory);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Armory.mineralPrice());
			setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Armory.gasPrice());
		}

		if (shouldExpand())
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
		}

		// Geysers
		if (numRaxFinished && getNumTotalUnit(BWAPI::UnitTypes::Terran_Refinery) < numGeyserBases() &&
			getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center) >= numGeyserBases() &&
			_ownedBases.size() > 1)
		{
			_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
			setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
		}
	}
}

/**************************************************
 **************************************************
 * END BUILD ORDER SECTION
 **************************************************
***************************************************/

void insanitybot::InformationManager::onUnitShow(BWAPI::Unit unit)
{
	if (!unit || !unit->exists())
		return;

	if (unit->getPlayer() == _enemy)
	{
		if (_enemyRace == BWAPI::Races::Unknown)
		{
			_enemyRace = unit->getType().getRace();

			if (_enemyRace == BWAPI::Races::Zerg && !isBio(_strategy))
			{
				_strategy = "Nuke";
			}
		}

		if (!_enemyHasAir)
		{
			if (unit->isFlying() && !unit->getType().isBuilding() && 
				unit->getType() != BWAPI::UnitTypes::Zerg_Overlord)
				_enemyHasAir = true;
		}

		if (unit->getType().isBuilding())
		{
			bool found = false;
			for (auto structure : _enemyStructurePositions)
			{
				if (structure == unit->getPosition())
				{
					found = true;
					break;
				}
			}

			if (!found)
				_enemyStructurePositions.push_back(unit->getPosition());

			if (unit->getType() == BWAPI::UnitTypes::Zerg_Spawning_Pool && !_enemyPool)
				_enemyPool = true;
		}

		if (unit->getType().isResourceDepot())
		{
			for (auto & base : _otherBases)
			{
				if (closeEnough(unit->getPosition(), base.first))
				{
					base.second->setbaseCommandCenter(unit);
					_enemyBases.insert(std::pair<BWAPI::Position, BWEM::Base *>(base.first, base.second));
					_otherBases.erase(base.first);
					return;
				}
			}

			if (_islandBases.size())
			{
				for (auto & base : _islandBases)
				{
					if (closeEnough(unit->getPosition(), base.first))
					{
						base.second->setbaseCommandCenter(unit);
						_enemyBases.insert(std::pair<BWAPI::Position, BWEM::Base *>(base.first, base.second));
						_islandBases.erase(base.first);
						return;
					}
				}
			}
		}
		else
		{
			if (!_enemyUnits.contains(unit))
			{
				_enemyUnits.insert(unit);
			}
		}
	}
}


void insanitybot::InformationManager::onUnitDestroy(BWAPI::Unit unit)
{
	if (unit->getType().isMineralField())
	{
		// Is it in our list of low resource minerals?
		for (auto mineral : _smallMinerals)
		{
			if (mineral == unit)
			{
				_smallMinerals.erase(mineral);
				break;
			}

		}
		
		// Is it in our list of low island resource minerals?
		for (auto mineral : _islandSmallMinerals)
		{
			if (mineral == unit)
			{
				_islandSmallMinerals.erase(mineral);
				break;
			}

		}

		// ToDo: Clean this up. There should be an easier way to delete minerals from inside of individual bases
		for (auto & base : _ownedBases)
		{
			if (base.second->getDestroyedMineral(unit))
				base.second->OnMineralDestroyed(base.second->getDestroyedMineral(unit));
		}

		for (auto & base : _otherBases)
		{
			if (base.second->getDestroyedMineral(unit))
				base.second->OnMineralDestroyed(base.second->getDestroyedMineral(unit));
		}

		for (auto & base : _islandBases)
		{
			if (base.second->getDestroyedMineral(unit))
				base.second->OnMineralDestroyed(base.second->getDestroyedMineral(unit));
		}

		for (auto & base : _ownedIslandBases)
		{
			if (base.second->getDestroyedMineral(unit))
				base.second->OnMineralDestroyed(base.second->getDestroyedMineral(unit));
		}
	}
	else if (unit->getPlayer() == BWAPI::Broodwar->neutral())
	{
		if (unit->getType().isBuilding())
		{
			for (std::list<BWAPI::Unit>::iterator building = _neutralBuildings.begin(); building != _neutralBuildings.end(); building++)
			{
				if (*building == unit)
				{
					_neutralBuildings.erase(building);
					return;
				}
			}
		}
	}
}

void insanitybot::InformationManager::onUnitRenegade(BWAPI::Unit unit)
{
	if (!unit || !unit->exists())
		return;

	if (unit->getType().isRefinery() && unit->getPlayer() == _self)
	{
		for (auto base : _ownedBases)
		{
			if (base.second->baseHasGeyser() && abs(BWAPI::TilePosition(unit->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition(unit->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
			{
				_ownedBases[base.first]->setBaseRefinery(unit);
				return;
			}
		}

		for (auto base : _ownedIslandBases)
		{
			if (base.second->baseHasGeyser() && abs(BWAPI::TilePosition(unit->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition(unit->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
			{
				_ownedIslandBases[base.first]->setBaseRefinery(unit);
				return;
			}
		}
		BWAPI::Broodwar << "No base found for built geyser" << std::endl;
	}

	if (unit->getType().isRefinery() && unit->getPlayer() == _enemy)
	{
		for (auto base : _enemyBases)
		{
			if (base.second->baseHasGeyser() && abs(BWAPI::TilePosition(unit->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition(unit->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
			{
				_enemyBases[base.first]->setBaseRefinery(unit);
				return;
			}
		}

		for (auto base : _otherBases)
		{
			if (base.second->baseHasGeyser() && abs(BWAPI::TilePosition(unit->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition(unit->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
			{
				_otherBases[base.first]->setBaseRefinery(unit);
				return;
			}
		}
	}
}

void insanitybot::InformationManager::onUnitComplete(BWAPI::Unit unit)
{
	
}

// Quick check to see if we need another expansion
bool insanitybot::InformationManager::shouldExpand()
{
	if (_otherBases.size() == 0)
		return false;
	//else if (_self->minerals() > 1000)
	//	return true;

	int baseAbove = 0;
	for (auto base : _ownedBases)
	{
		if (base.second->getRemainingMinerals() > 1500)
		{
			baseAbove += 1;
		}
	}
	if (baseAbove < 2 && isBio(_strategy))
		return true;
	else if (baseAbove < 3 && isMech(_strategy))
		return true;
	else if (baseAbove < 1 && isAllIn(_strategy))
		return true;
	else
		return false;
}

// Quick check to see if we are currently expanding
bool insanitybot::InformationManager::isExpanding()
{
	for (auto item : _queue)
	{
		if (item.isResourceDepot())
		{
			return true;
		}
	}
	
	return false;
}

// A check to see if our main base needs a defensive squad
bool insanitybot::InformationManager::shouldHaveDefenseSquad(bool worker)
{
	for (auto unit : _enemy->getUnits())
	{
		if (!unit)
			continue;

		if (unit->exists() && unit->getDistance(BWAPI::Position(_mainPosition)) < 800 && unit->getType() != BWAPI::UnitTypes::Zerg_Overlord &&
			unit->getType() != BWAPI::UnitTypes::Protoss_Observer)
			return true;
	}

	if (BWAPI::Broodwar->getFrameCount() > 20000 && !worker)
	{
		for (auto base : _ownedBases)
		{
			for (auto structure : _neutralBuildings)
			{
				if (structure->getDistance(base.first) < 800)
				{
					return true;
				}
			}
		}
	}

	return false;
}

// A check to see if we're expanding and need a frontier squad.
int insanitybot::InformationManager::numFrontierSquadsNeeded()
{
	int numSquadsWanted = -2;
	numSquadsWanted += _ownedBases.size();

	if (isExpanding() && !_islandExpand)
		return numSquadsWanted + 1;
	else
		return numSquadsWanted;
}


/********************************************************************
* A simple check to see if a rush is happening
*********************************************************************/
bool insanitybot::InformationManager::checkForEnemyRush()
{
	if (_enemyRace == BWAPI::Races::Zerg)
	{
		if (_enemyPool)
		{
			int numWorker = 0;

			for (auto unit : _enemyUnits)
			{
				if (unit->getType().isWorker())
					numWorker += 1;
			}

			if (numWorker <= 5)
				return true;
		}
		
		if (_enemyUnits.size())
		{
			int numHatch = 0;
			int numZerglings = 0;

			for (auto unit : _enemyUnits)
			{
				if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling)
					numZerglings += 1;
				else if (unit->getType().isResourceDepot())
					numHatch += 1;
			}

			if (numHatch > 1 || _enemyBases.size() > 1)
				return false;
			else if (numZerglings >= 4)
				return true;
		}
	}
	else if (_enemyRace == BWAPI::Races::Protoss)
	{
		if (_enemyUnits.size())
		{
			int numGateways = 0;
			int numNexus = 0;

			for (auto unit : _enemyUnits)
			{
				if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus)
					numNexus += 1;
				else if (unit->getType() == BWAPI::UnitTypes::Protoss_Gateway)
					numGateways += 1;
				else if (unit->getType() == BWAPI::UnitTypes::Protoss_Pylon)
				{
					if (unit && unit->exists())
					{
						if (unit->getDistance(_mainChoke) < 1500)
						{
							BWAPI::Broodwar << "Close Pylon Detected" << std::endl;
							return true;
						}
					}
				}
			}

			if (numNexus > 1)
				return false;
			else if (numGateways >= 2)
				return true;
		}
	}
	else if (_enemyRace == BWAPI::Races::Terran)
	{
		if (_enemyUnits.size())
		{
			int numRax = 0;
			int numCC = 0;

			for (auto unit : _enemyUnits)
			{
				if (unit->getType() == BWAPI::UnitTypes::Terran_Command_Center)
					numCC += 1;
				else if (unit->getType() == BWAPI::UnitTypes::Terran_Barracks)
					numRax += 1;
			}

			if (numCC > 1)
				return false;
			else if (numRax >= 2)
				return true;
		}
	}

	return false;
}

std::vector<BWAPI::Unit> insanitybot::InformationManager::mineralsNeedClear()
{
	std::vector<BWAPI::Unit> mineralsNeedClearing;
	for (auto base : _ownedBases)
	{
		for (auto mineral : _smallMinerals)
		{
			if (mineral->getDistance(base.first) < 1000)
			{
				mineralsNeedClearing.push_back(mineral);
			}
		}
	}

	return mineralsNeedClearing;
	
}

bool insanitybot::InformationManager::closeEnough(BWAPI::Position location1, BWAPI::Position location2)
{
	// Used for assigning bases to locations. If the coordinates are "close enough", we call it good.
	return abs(location1.x - location2.x) <= 64 && abs(location1.y - location2.y) <= 64;
}


int insanitybot::InformationManager::getNumFinishedUnit(BWAPI::UnitType type)
{
	// This function will loop through our units and return the total number of finished
	// units of a given type we own. It will not include units still constructing.
	int count = 0;
	for (auto & unit : _self->getUnits())
	{
		if (!unit)
			continue;

		if (unit->getType() == type && unit->isCompleted() && unit->exists())
		{
			count += 1;
		}
	}
	return count;
}

int insanitybot::InformationManager::getNumUnfinishedUnit(BWAPI::UnitType type)
{
	// This function will loop through our units and return the total number of unfinished
	// units of a given type we own.
	int count = 0;
	for (auto & unit : _self->getUnits())
	{
		if (!unit)
			continue;

		if (unit->getType() == type && !unit->isCompleted() && unit->exists())
		{
			count += 1;
		}
	}
	return count;
}

int insanitybot::InformationManager::getNumTotalUnit(BWAPI::UnitType type)
{
	// This function will loop through our units and return the total number of units
	// we own of a given type. Finished or still constructing.
	int count = 0;
	for (auto unit : _self->getUnits())
	{
		if (!unit)
			continue;

		if (unit->getType() == type && unit->exists())
		{
			count += 1;
		}
	}
	return count;
}

int insanitybot::InformationManager::numGeyserBases()
{
	int numWanted = 0;
	for (auto & base : _ownedBases)
	{
		numWanted += base.second->baseHasGeyser();
	}

	return numWanted;
}

int insanitybot::InformationManager::numWorkersWanted()
{
	// Loop through each base and return the number of mineral patches * 3.
	// Roughly what we want mining, excluding refineries for the time being
	int numWanted = 0;
	for (auto & base : _ownedBases)
	{
		//numWanted += base.second->Minerals().size() * 3;
		numWanted += base.second->numWorkersWantedHere();
	}

	// We only want around 70 workers active at any given time. May bump up to 75 later but we'll see
	if (numWanted > 70)
		return 70;
	else if (numWanted < 3)
		return 3 * getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center);
	else
		return numWanted;
}

bool InformationManager::targetIsDefended()
{
	if (_strategy != "Nuke" || !_enemyBases.size())
		return false;

	if (_targetDefended)
		return true;

	int numSunks = 0;
	BWAPI::Position target = _enemyBases.begin()->first;

	if (closeEnough(target, BWAPI::Position(_enemyMainPos)))
	{
		for (auto enemyBase : _enemyBases)
		{
			if (closeEnough(enemyBase.first, _enemyNatPos))
			{
				target = enemyBase.first;
				break;
			}
		}
	}

	for (auto enemyUnit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (enemyUnit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony && !enemyUnit->isMorphing() && enemyUnit->getDistance(target) < 800)
		{
			numSunks++;
		}
	}

	if (numSunks > 1)
	{
		_targetDefended = true;
		return true;
	}
	else
		return false;
}

InformationManager & InformationManager::Instance()
{
	static InformationManager instance;
	return instance;
}