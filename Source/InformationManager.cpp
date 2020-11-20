#include "InformationManager.h"


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

	if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
	{
		_strategy = "SKTerran";
	}
	else if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran || BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss)
	{
		_strategy = "Mech";
	}
	else // Random
	{
		_strategy = "Mech";
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

	_enemyNatPos = BWAPI::Position(0, 0);

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
		if (!u)
			continue;

		if (u->getType().isWorker())
		{
			_workers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(u, _ownedBases.begin()->second));
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
}

/****************************************************
* Update is called each frame (close enough to each frame,
* see GameCommander), and will set our aggression level,
* initial scouting, and update our build order
*****************************************************/
void InformationManager::update()
{
	if (_strategy == "SKTerran" && _marines.size() > 30)
	{
		setAggression(true);
	}
	else if (_strategy == "Mech" && _tanks.size() >= 7)
	{
		setAggression(true);
	}

	if (_enemyNatPos == BWAPI::Position(0, 0) && _enemyBases.size())
	{
		// Setup enemy Main Location
		_enemyMainPos = _enemyBases.begin()->second->Location();

		// Setup enemy natural location
		int shortest = 999999;
		int length = -1;
		for (auto base : _otherBases)
		{
			if (base.second->baseHasGeyser())
			{
				BWEM::CPPath temp = theMap.GetPath(BWAPI::Position(base.first), BWAPI::Position(_enemyBases.begin()->first), &length);

				if (length < 0) continue;

				if (length < shortest)
				{
					shortest = length;
					_enemyNatPos = base.first;
				}
			}
		}
	}

	//Initial Scouting
	if (_enemyBases.size() < 1 && _workers.size() >= 7 && _scout == NULL && BWAPI::Broodwar->getFrameCount() < 20000)
	{
		for (std::map<BWAPI::Unit, BWEM::Base *>::iterator & it = _workers.begin(); it != _workers.end(); it++)
		{
			if (it->first->getType() != BWAPI::UnitTypes::Terran_SCV)
			{
				BWAPI::Broodwar << it->first->getType() << " found in worker list." << std::endl;
				continue;
			}

			if (it->first->getType().isWorker() && (it->first->isIdle() || it->first->isGatheringMinerals())
				&& !it->first->isCarryingGas() && !it->second->isGasWorker(it->first) && !it->first->isConstructing())
			{
				_scout = it->first;
				_workers.erase(it);
				it->second->removeAssignment(it->first);
				break;
			}
		}
	}
	else if (_enemyBases.size() && _scout != NULL)
	{
		_workers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(_scout, _ownedBases.begin()->second));
		_scout = NULL;
	}

	//Update build order
	updateBuildOrder();

	//Clear the lists of units
	_construction.clear();
	_turrets.clear();
	_injuredBuildings.clear();
	_floatingBuildings.clear();

	//Loop through our units and add constructing buildings to our list
	for (BWAPI::Unit myUnit : _self->getUnits())
	{
		if (!myUnit || !myUnit->exists())
			continue;

		if (myUnit->getType() == BWAPI::UnitTypes::Terran_Missile_Turret && myUnit->isCompleted())
		{
			_turrets.push_back(myUnit);
		}

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

			for (auto u : BWAPI::Broodwar->enemy()->getUnits()) {

				if (!u || !u->exists())
					continue;

				if ((u->getType().isBuilding()) && (u->getPosition() == structure)) {
					stillThere = true;
					break;
				}
			}

			// if there is no building, remove that from our list
			if (stillThere == false) {
				_enemyStructurePositions.remove(structure);
				break;
			}
		}
	}


	// Attempted fix for canceled zerg structures not clearing tracked enemy bases
	for (auto base : _enemyBases)
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

			if (unit->isVisible() && !unit->isDetected())
			{
				for (auto com : _comsats)
				{
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
	int numRaxFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Barracks);
	int numFactoryFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Factory);
	int numStarportFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Starport);
	int numEngiBaysFinished = getNumFinishedUnit(BWAPI::UnitTypes::Terran_Engineering_Bay);
	bool hasAcademy = _academy.size();
	bool hasScience = _science.size();
	/******************************************************
	 * Important section: Very primative build order. This
	 * is where we will decide what needs to be built and
	 * minerals/gas will be reserved for it.
	 ******************************************************/
	if (_queue.empty())
	{
		/***************************************************
		* SKTerran: medics marines and vessels. Might add 
		*           firebats later for dark swarm
		****************************************************/
		if (_strategy == "SKTerran")
		{
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
				if (numRaxFinished == 1 && (_ownedBases.size() >= 2 || _self->deadUnitCount(BWAPI::UnitTypes::Terran_Command_Center) > 1) && _barracks.size() == 1
					&& _marines.size() > 2)
				{
					_queue.push_back(BWAPI::UnitTypes::Terran_Barracks);
					setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Barracks.mineralPrice());
				}

				// 3rd rax, academy, and engibay
				if (numRaxFinished == 2 && _barracks.size() == 2 && _marines.size() > 8)
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
				if (hasAcademy && _factories.size() < 1 && _marines.size() > 16)
				{
					_queue.push_back(BWAPI::UnitTypes::Terran_Factory);
					setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Factory.mineralPrice());
					setReservedGas(getReservedGas() + BWAPI::UnitTypes::Terran_Factory.gasPrice());
				}

				// 5 rax and port
				if (numFactoryFinished >= 1 && _marines.size() > 24 && _starports.size() == 0)
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
					_queue.push_back(BWAPI::UnitTypes::Terran_Bunker);
					setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Bunker.mineralPrice());
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

			if (hasAcademy && getNumTotalUnit(BWAPI::UnitTypes::Terran_Refinery) < numGeyserBases() &&
				_commandCenters.size() >= numGeyserBases())
			{
				_queue.push_back(BWAPI::UnitTypes::Terran_Refinery);
				setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Refinery.mineralPrice());
			}
		}
		else if (_strategy == "Mech")
		{
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
				else if (numFactoryFinished == 2 && _factories.size() == 2 && _tanks.size() > 3)
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
				else if (numFactoryFinished < 5 && _factories.size() < 5 && _tanks.size() > 4)
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
				}
				else if (_ownedBases.size() >= 2 && shouldExpand() && (_self->minerals() > 800 || BWAPI::Broodwar->getFrameCount() > 20000))
				{
					_queue.push_back(BWAPI::UnitTypes::Terran_Command_Center);
					setReservedMinerals(getReservedMinerals() + BWAPI::UnitTypes::Terran_Command_Center.mineralPrice());
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

		//Island base refinery and turrets
		if (_ownedIslandBases.size())
		{
			for (auto base : _ownedIslandBases)
			{
				if (base.second->baseHasGeyser() && !base.second->baseHasRefinery())
				{
					for (auto worker : _islandWorkers)
					{
						if (worker.second == base.second && worker.first->isGatheringMinerals())
						{
							if (_self->minerals() >= 100)
							{
								worker.first->build(BWAPI::UnitTypes::Terran_Refinery, base.second->Geysers().front()->TopLeft());
							}
							break;
						}
						else if (worker.second == base.second && base.second->numTurretsHere() < 3 && worker.first->isGatheringGas() && _engibays.size())
						{
							if (_self->minerals() >= 75)
							{
								worker.first->build(BWAPI::UnitTypes::Terran_Missile_Turret, BWAPI::Broodwar->getBuildLocation(BWAPI::UnitTypes::Terran_Missile_Turret, base.second->Location()));
							}
						}
					}
				}
				else if (base.second->numTurretsHere() < 3 && _engibays.size())
				{
					for (auto worker : _islandWorkers)
					{
						if (worker.second == base.second && worker.first->isGatheringGas() )
						{
							if (_self->minerals() >= 75)
							{
								worker.first->build(BWAPI::UnitTypes::Terran_Missile_Turret, BWAPI::Broodwar->getBuildLocation(BWAPI::UnitTypes::Terran_Missile_Turret, base.second->Location()));
								break;
							}
						}
					}
				}
			}
		}

		if (numEngiBaysFinished && getNumTotalUnit(BWAPI::UnitTypes::Terran_Missile_Turret) < getNumFinishedUnit(BWAPI::UnitTypes::Terran_Command_Center) * 3 &&
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

void insanitybot::InformationManager::onUnitShow(BWAPI::Unit unit)
{
	if (!unit || !unit->exists())
		return;

	if (unit->getPlayer() == _enemy)
	{
		if (_enemyRace == BWAPI::Races::Unknown)
		{
			_enemyRace = unit->getType().getRace();

			if (_enemyRace == BWAPI::Races::Zerg && _strategy != "SKTerran")
			{
				_strategy = "SKTerran";
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
	}
}

void insanitybot::InformationManager::onUnitCreate(BWAPI::Unit unit)
{
	if (!unit || !unit->exists())
		return;

	// Insert workers that are created to our list. 
	if (unit->getType().isWorker() && unit->getPlayer() == _self)
	{
		for (auto base : _ownedBases)
		{
			if (closeEnough(unit->getPosition(), base.first))
			{
				_workers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(unit, base.second));
				return;
			}
		}

		for (auto base : _ownedIslandBases)
		{
			if (closeEnough(unit->getPosition(), base.first))
			{
				_islandWorkers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(unit, base.second));
				return;
			}
		}
	}
	// Insert Marines
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Marine && unit->getPlayer() == _self)
	{
		_marines.insert(std::pair<BWAPI::Unit, int>(unit, 0));
	}
	// Medics
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Medic && unit->getPlayer() == _self)
	{
		_medics.insert(std::pair<BWAPI::Unit, int>(unit, 0));
	}
	// Dropships
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Dropship && unit->getPlayer() == _self)
	{
		_dropships.insert(std::pair<BWAPI::Unit, int>(unit, 0));
	}
	// Vessels
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Science_Vessel && unit->getPlayer() == _self)
	{
		_vessels.insert(std::pair<BWAPI::Unit, int>(unit, 0));
	}
	// Vultuers
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture && unit->getPlayer() == _self)
	{
		_vultures.insert(std::pair<BWAPI::Unit, int>(unit, 0));
	}
	// Tanks
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode && unit->getPlayer() == _self)
	{
		_tanks.insert(std::pair<BWAPI::Unit, int>(unit, 0));
	}
	// Goliaths
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Goliath && unit->getPlayer() == _self)
	{
		_goliaths.insert(std::pair<BWAPI::Unit, int>(unit, 0));
	}
	// Barracks
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Barracks && unit->getPlayer() == _self)
	{
		_barracks.push_back(unit);
	}
	// Factory
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Factory && unit->getPlayer() == _self)
	{
		_factories.push_back(unit);
	}
	// Starport
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Starport && unit->getPlayer() == _self)
	{
		_starports.push_back(unit);
	}
	// Academy
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Academy && unit->getPlayer() == _self)
	{
		_academy.push_back(unit);
	}
	// Engibays
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Engineering_Bay && unit->getPlayer() == _self)
	{
		_engibays.push_back(unit);
	}
	// Armory
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Armory && unit->getPlayer() == _self)
	{
		_armories.push_back(unit);
	}
	// Science Facility
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Science_Facility && unit->getPlayer() == _self)
	{
		_science.push_back(unit);
	}
	// Addons
	else if (unit->getType().isAddon() && unit->getPlayer() == _self)
	{
		if (unit->getType() == BWAPI::UnitTypes::Terran_Comsat_Station)
		{
			_comsats.push_back(unit);
		}
		else if (unit->getType() == BWAPI::UnitTypes::Terran_Machine_Shop)
		{
			_machineShops.push_back(unit);
		}
		else
		{
			_addons.push_back(unit);
		}
	}
	// Turrets
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Missile_Turret && unit->getPlayer() == _self)
	{
		for (auto & base : _ownedBases)
		{
			if (unit->getDistance(base.first) < 400)
			{
				base.second->addTurrets(unit);
				break;
			}
		}

		for (auto & base : _ownedIslandBases)
		{
			if (unit->getDistance(base.first) < 400)
			{
				base.second->addTurrets(unit);
				break;
			}
		}
	}
	// Insert command center as new base being taken
	else if (unit->getType().isResourceDepot() && unit->getPlayer() == _self)
	{
		for (auto & base : _otherBases)
		{
			if (closeEnough(unit->getPosition(), base.first))
			{
				_commandCenters.push_back(unit);
				base.second->setbaseCommandCenter(unit);
				_ownedBases.insert(std::pair<BWAPI::Position, BWEM::Base *>(base.first, base.second));
				_otherBases.erase(base.first);
				return;
			}
		}

		for (auto & base : _islandBases)
		{
			if (closeEnough(unit->getPosition(), base.first))
			{
				_islandWorkers.insert(std::pair<BWAPI::Unit, BWEM::Base *>(unit->getBuildUnit(), base.second));
				_islandBuilder = NULL;
				_islandCenters.push_back(unit);
				base.second->setbaseCommandCenter(unit);
				_ownedIslandBases.insert(std::pair<BWAPI::Position, BWEM::Base *>(base.first, base.second));
				_islandBases.erase(base.first);
				return;
			}
		}
	}
	// Refineries should be assigned to their respective bases
	else if (unit->getType().isRefinery() && unit->getPlayer() == _self)
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
	}
	// Bunkers are tracked seperately for repair purposes
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Bunker && unit->getPlayer() == _self)
	{
		_bunkers.push_back(unit);
	}
}

void insanitybot::InformationManager::onUnitDestroy(BWAPI::Unit unit)
{
	if (!unit)
		return;

	// Remove killed workers from our worker list.
	if (unit->getType().isWorker() && unit->getPlayer() == _self)
	{
		// Is it our scout?
		if (unit == _scout)
		{
			if (!_enemyBases.size())
			{
				int closest = 99999;
				for (auto location : theMap.StartingLocations())
				{
					if (unit->getDistance(BWAPI::Position(location)) < closest && !closeEnough(BWAPI::Position(location), _home->Center()))
					{
						closest = unit->getDistance(BWAPI::Position(location));

						for (auto base : _otherBases)
						{
							if (closeEnough(BWAPI::Position(location), base.first))
							{
								_ownedBases.insert(std::pair<BWAPI::Position, BWEM::Base *>(base.first, base.second));
								_otherBases.erase(base.first);
								break;
							}
						}
					}
				}
			}

			_scout = NULL;
			return;
		}

		//If not, erase it from our list and remove its assignment
		if (_workers.find(unit) != _workers.end())
		{
			_workers.erase(unit);

			for (auto base : _ownedBases)
			{
				base.second->onUnitDestroy(unit);
			}
		}
	}
	// Remove dead marines
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Marine && unit->getPlayer() == _self)
	{
		_marines.erase(unit);
	}
	// Remove dead medics
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Medic && unit->getPlayer() == _self)
	{
		_medics.erase(unit);
	}
	// Remove dead dropships
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Dropship && unit->getPlayer() == _self)
	{
		_dropships.erase(unit);
	}
	// Remove dead vessels
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Science_Vessel && unit->getPlayer() == _self)
	{
		_vessels.erase(unit);
	}
	// Remove dead vultures
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture && unit->getPlayer() == _self)
	{
		_vultures.erase(unit);
	}
	// Remove dead tanks
	else if ((unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode || unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) && unit->getPlayer() == _self)
	{
		_tanks.erase(unit);
	}
	// Remove dead goliaths
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Goliath && unit->getPlayer() == _self)
	{
		_goliaths.erase(unit);
	}
	// Barracks
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Barracks && unit->getPlayer() == _self)
	{
		for (std::list<BWAPI::Unit>::iterator rax = _barracks.begin(); rax != _barracks.end(); rax++)
		{
			if (*rax == unit)
			{
				_barracks.erase(rax);
				break;
			}
		}
	}
	// Factory
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Factory && unit->getPlayer() == _self)
	{
		for (std::list<BWAPI::Unit>::iterator factory = _factories.begin(); factory != _factories.end(); factory++)
		{
			if (*factory == unit)
			{
				_factories.erase(factory);
				break;
			}
		}
	}
	// Starport
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Starport && unit->getPlayer() == _self)
	{
		for (std::list<BWAPI::Unit>::iterator starport = _starports.begin(); starport != _starports.end(); starport++)
		{
			if (*starport == unit)
			{
				_starports.erase(starport);
				break;
			}
		}
	}
	// Academy
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Academy && unit->getPlayer() == _self)
	{
		for (std::list<BWAPI::Unit>::iterator academy = _academy.begin(); academy != _academy.end(); academy++)
		{
			if (*academy == unit)
			{
				_academy.erase(academy);
				break;
			}
		}
	}
	// Engibays
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Engineering_Bay && unit->getPlayer() == _self)
	{
		for (std::list<BWAPI::Unit>::iterator engibay = _engibays.begin(); engibay != _engibays.end(); engibay++)
		{
			if (*engibay == unit)
			{
				_engibays.erase(engibay);
				break;
			}
		}
	}
	// Armories
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Armory && unit->getPlayer() == _self)
	{
		for (std::list<BWAPI::Unit>::iterator armory = _armories.begin(); armory != _armories.end(); armory++)
		{
			if (*armory == unit)
			{
				_armories.erase(armory);
				break;
			}
		}
	}
	// Science Facility
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Science_Facility && unit->getPlayer() == _self)
	{
		for (std::list<BWAPI::Unit>::iterator science = _science.begin(); science != _science.end(); science++)
		{
			if (*science == unit)
			{
				_science.erase(science);
				break;
			}
		}
	}
	// Addons
	else if (unit->getType().isAddon() && unit->getPlayer() == _self)
	{
		if (unit->getType() == BWAPI::UnitTypes::Terran_Comsat_Station)
		{
			for (std::list<BWAPI::Unit>::iterator comsat = _comsats.begin(); comsat != _comsats.end(); comsat++)
			{
				if (*comsat == unit)
				{
					_comsats.erase(comsat);
					break;
				}
			}
		}
		else if (unit->getType() == BWAPI::UnitTypes::Terran_Machine_Shop)
		{
			for (std::list<BWAPI::Unit>::iterator shop = _machineShops.begin(); shop != _machineShops.end(); shop++)
			{
				if (*shop == unit)
				{
					_machineShops.erase(shop);
					break;
				}
			}
		}
		else
		{
			for (std::list<BWAPI::Unit>::iterator addon = _addons.begin(); addon != _addons.end(); addon++)
			{
				if (*addon == unit)
				{
					_addons.erase(addon);
					break;
				}
			}
		}
	}
	// Turrets
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Missile_Turret && unit->getPlayer() == _self)
	{
		for (auto & base : _ownedBases)
		{
			if (base.second->onTurretDestroy(unit))
			{
				break;
			}
		}
	}
	// Remove lost bases from our owned bases list
	else if (unit->getType().isResourceDepot())
	{
		if (unit->getPlayer() == _self)
		{
			for (auto base : _ownedBases)
			{
				if (closeEnough(unit->getPosition(), base.first))
				{
					for (auto & worker : _workers)
					{
						if (worker.second == base.second)
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
					
					for (std::list<BWAPI::Unit>::iterator center = _commandCenters.begin(); center != _commandCenters.end(); center++)
					{
						if (*center == unit)
						{
							_commandCenters.erase(center);
							break;
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

					base.second->setbaseCommandCenter(NULL);
					base.second->clearAssignmentList();
					_otherBases.insert(std::pair<BWAPI::Position, BWEM::Base *>(base.first, base.second));
					_ownedBases.erase(base.first);
					return;
				}
			}

			for (auto base : _ownedIslandBases)
			{
				if (closeEnough(unit->getPosition(), base.first))
				{
					for (auto & worker : _islandWorkers)
					{
						if (worker.second == base.second)
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

					for (std::list<BWAPI::Unit>::iterator center = _islandCenters.begin(); center != _islandCenters.end(); center++)
					{
						if (*center == unit)
						{
							_islandCenters.erase(center);
							break;
						}
					}

					base.second->setbaseCommandCenter(NULL);
					base.second->clearAssignmentList();
					_islandBases.insert(std::pair<BWAPI::Position, BWEM::Base *>(base.first, base.second));
					_ownedIslandBases.erase(base.first);
					return;
				}
			}
		}
		else if (unit->getPlayer() == _enemy)
		{
			for (auto base : _enemyBases)
			{
				if (closeEnough(unit->getPosition(), base.first))
				{
					base.second->setbaseCommandCenter(NULL);
					base.second->clearAssignmentList();
					_otherBases.insert(std::pair<BWAPI::Position, BWEM::Base *>(base.first, base.second));
					_enemyBases.erase(base.first);
					return;
				}
			}
		}
	}
	else if (unit->getType().isMineralField())
	{
		// Is it in our list of low resource minerals?
		for (auto mineral : _smallMinerals)
		{
			if (mineral == unit)
			{
				_smallMinerals.erase(mineral);
			}

		}
		
		// Is it in our list of low island resource minerals?
		for (auto mineral : _islandSmallMinerals)
		{
			if (mineral == unit)
			{
				_islandSmallMinerals.erase(mineral);
			}

		}

		// ToDo: Clean this up. There should be an easier way to delete minerals from inside of individual bases
		for (auto base : _ownedBases)
		{
			base.second->OnMineralDestroyed(base.second->getDestroyedMineral(unit));
		}

		for (auto base : _otherBases)
		{
			base.second->OnMineralDestroyed(base.second->getDestroyedMineral(unit));
		}

		for (auto base : _islandBases)
		{
			base.second->OnMineralDestroyed(base.second->getDestroyedMineral(unit));
		}

		for (auto base : _ownedIslandBases)
		{
			base.second->OnMineralDestroyed(base.second->getDestroyedMineral(unit));
		}
	}
	else if (unit->getType().isRefinery() && unit->getPlayer() == _self)
	{
		for (auto base : _ownedBases)
		{
			if (base.second->Geysers().size() && abs(BWAPI::TilePosition(unit->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition(unit->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
			{
				_ownedBases[base.first]->setBaseRefinery(NULL);
				return;
			}
		}
		
		for (auto base : _otherBases)
		{
			if (base.second->Geysers().size() && abs(BWAPI::TilePosition(unit->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition(unit->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
			{
				_otherBases[base.first]->setBaseRefinery(NULL);
				return;
			}
		}

		for (auto base : _islandBases)
		{
			if (base.second->Geysers().size() && abs(BWAPI::TilePosition(unit->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition(unit->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
			{
				_ownedBases[base.first]->setBaseRefinery(NULL);
				return;
			}
		}

		for (auto base : _ownedIslandBases)
		{
			if (base.second->Geysers().size() && abs(BWAPI::TilePosition(unit->getPosition()).x - base.second->Geysers().front()->TopLeft().x) <= 8 && abs(BWAPI::TilePosition(unit->getPosition()).y - base.second->Geysers().front()->TopLeft().y) <= 8)
			{
				_otherBases[base.first]->setBaseRefinery(NULL);
				return;
			}
		}
	}
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Bunker && unit->getPlayer() == _self)
	{
		for (auto bunker : _bunkers)
		{
			if (bunker == unit)
			{
				_bunkers.remove(bunker);
				return;
			}
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
	if (!unit)
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
	if (baseAbove < 2 && _strategy == "SKTerran")
		return true;
	else if (baseAbove < 3 && _strategy == "Mech")
		return true;
	else
		return false;
}

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

InformationManager & InformationManager::Instance()
{
	static InformationManager instance;
	return instance;
}