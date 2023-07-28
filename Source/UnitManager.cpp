#include "UnitManager.h"
#include "BuildingPlacer.h"
#include <math.h>

namespace { auto & theMap = BWEM::Map::Instance(); }

using namespace insanitybot;

insanitybot::UnitManager::UnitManager()
{
	infantrySquadSizeLimit = 15;
	mechSquadSizeLimit = 9;
	specialistSquadSizeLimit = 4;
	frontierSquadSizeLimit = 5;
	dropSquadSizeLimit = 8;
	numEmptyBases = 0;

	_direction = 0;
	_nextWaypointIndex = -1;
	_lastWaypointIndex = -1;

	needDefense = false;
	loadingDrop = false;
	dropping = false;

	scoutNextTarget = BWAPI::TilePosition(0, 0);
	circlePositions.clear();
	_waypoints.clear();
}

void UnitManager::update(InformationManager & _infoManager)
{
	bool stratIsBio = _infoManager.isBio(_infoManager.getStrategy());
	bool stratIsMech = _infoManager.isMech(_infoManager.getStrategy());
	bool stratIsAir = _infoManager.isAirStrat(_infoManager.getStrategy());
	bool stratIsAllIn = _infoManager.isAllIn(_infoManager.getStrategy());
	bool targetIsDefended = _infoManager.targetIsDefended();


	// If we don't know where the enemy is, scout around
	std::vector<BWAPI::TilePosition>& squadScoutLocation = _infoManager.getSquadScoutLocations();
	BWAPI::TilePosition nextUp = squadScoutLocation.front();
	if (BWAPI::Broodwar->isVisible(nextUp.x, nextUp.y))
	{
		squadScoutLocation.push_back(squadScoutLocation.front());
		squadScoutLocation.erase(squadScoutLocation.begin());
		nextUp = squadScoutLocation.front();
	}

	bool needFieldEngineers = ((stratIsMech) || stratIsAllIn) && 
		_infoManager.getTanks().size() > 2 && _infoManager.getWorkers().size() > 15;

	if (needFieldEngineers && _infoManager.getFieldEngineers().size() < 2)
	{
		assignFieldEngineers(_infoManager);
	}
	
	checkDeadEngineers(_infoManager);

	if (_infoManager.getFieldEngineers().size())
	{
		handleFieldEngineers(_infoManager, nextUp);
	}


	if (stratIsBio)
		frontierSquadSizeLimit = 10;

	checkFlareDBForTimeout();

	if (_infoManager.getScout())
	{
		handleScout(_infoManager.getScout(), BWAPI::Position(_infoManager.getEnemyMainTilePos()));
	}

	if (stratIsBio)
	{	
		assignBio(_infoManager);
	}
	else if (stratIsMech)
	{	
		assignMech(_infoManager);
		handleFloaters(_infoManager, nextUp);
	}
	else if (stratIsAllIn)
	{
		assignAllIn(_infoManager);
	}

	if (stratIsAir)
	{
		assignAir(_infoManager);
	}

	// Loop through our ghosts and make sure they are assigned to a squad
	for (auto & ghost : _infoManager.getGhosts())
	{
		if (!ghost.first->exists())
		{
			continue;
		}

		if (ghost.second == 0)
		{
			if (assignSquad(ghost.first, false, false))
				ghost.second = 1;
		}
	}

	// Check if we need a defensive squad
	if (_infoManager.shouldHaveDefenseSquad(false) && !_defensiveSquads.size())
	{
		if (_infantrySquads.size())
		{
			for (std::list<Squad>::iterator & selection = _infantrySquads.begin(); selection != _infantrySquads.end(); selection++)
			{
				if (!selection->isGoodToAttack())
				{
					selection->setHaveGathered(true);
					selection->setDefenseLastNeededFrame(BWAPI::Broodwar->getFrameCount());
					_defensiveSquads.push_back(*selection);
					_infantrySquads.erase(selection);
					break;
				}
			}
		}
		else if (_mechSquads.size())
		{
			for (std::list<Squad>::iterator & selection = _mechSquads.begin(); selection != _mechSquads.end(); selection++)
			{
				if (!selection->isGoodToAttack())
				{
					if (!_infoManager.enemyHasAir() || (_infoManager.enemyHasAir() && selection->numGoliaths() > 1))
					{
						selection->setHaveGathered(true);
						selection->setDefenseLastNeededFrame(BWAPI::Broodwar->getFrameCount());
						_defensiveSquads.push_back(*selection);
						_mechSquads.erase(selection);
						break;
					}
				}
			}
		}
	}
	// Order the defensive squad around
	else if (_defensiveSquads.size())
	{
		BWAPI::Unit target = NULL;
		if (BWAPI::Broodwar->getFrameCount() > 5000)
		{
			for (auto base : _infoManager.getOwnedBases())
			{
				for (auto structure : _infoManager.getNeutralBuildings())
				{
					if (structure->getDistance(base.first) < 800)
					{
						target = structure;
					}
				}
			}
		}

		int closest = 999999;

		for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
		{
			if (!unit || !unit->exists())
				continue;

			int distance = unit->getDistance(BWAPI::Position(_infoManager.getMainPosition()));

			if (distance < 800)
			{
				if (distance < closest)
				{
					target = unit;
					closest = distance;
				}
			}
		}

		for (std::list<Squad>::iterator & squad = _defensiveSquads.begin(); squad != _defensiveSquads.end(); squad++)
		{
			if (target == NULL)
			{
				if (BWAPI::Broodwar->getFrameCount() - squad->getDefenseLastNeededFrame() > 1000)
				{
					if (stratIsBio)
					{
						squad->setHaveGathered(false);
						_infantrySquads.push_back(*squad);
						_defensiveSquads.erase(squad);
						break;
					}
					else if (stratIsMech)
					{
						squad->setHaveGathered(false);
						_mechSquads.push_back(*squad);
						_defensiveSquads.erase(squad);
						break;
					}
				}
				else
				{
					squad->attack(squad->getSquadPosition(), BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, BWAPI::Position(_infoManager.getMainPosition()).y), _flareBD);
				}
			}
			else if (target->getPlayer() == BWAPI::Broodwar->enemy())
			{
				if (stratIsBio)
				{
					// Send mostly full squads to engage
					if (squad->infantrySquadSize() >= 10)
					{
						squad->setDefenseLastNeededFrame(BWAPI::Broodwar->getFrameCount());
						squad->attack(target->getPosition(), BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, BWAPI::Position(_infoManager.getMainPosition()).y), _flareBD);
					}
					// Else, fall back to a defensive point and hope reinforcements pop soon
					else
					{
						squad->setDefenseLastNeededFrame(BWAPI::Broodwar->getFrameCount());
						BWAPI::Position defensePoint = BWAPI::Position((BWAPI::Position(_infoManager.getMainPosition()).x + target->getPosition().x) / 2, (BWAPI::Position(_infoManager.getMainPosition()).y + target->getPosition().y) / 2);
						squad->attack(defensePoint, BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, BWAPI::Position(_infoManager.getMainPosition()).y), _flareBD);
					}
				}
				else if (stratIsMech)
				{
					// Don't send squads with no anti air to defend air attacks
					if (target->getType().isFlyer() && squad->numGoliaths() < 1)
					{
						squad->setHaveGathered(false);
						_mechSquads.push_back(*squad);
						_defensiveSquads.erase(squad);
						break;
					}
					// Send mostly full squads to engage
					else if (squad->mechSquadSize() >= 7)
					{
						squad->setDefenseLastNeededFrame(BWAPI::Broodwar->getFrameCount());
						squad->attack(target->getPosition(), BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, BWAPI::Position(_infoManager.getMainPosition()).y), _flareBD);
					}
					// Else, fall back to a defensive point and hope reinforcements pop soon
					else
					{
						squad->setDefenseLastNeededFrame(BWAPI::Broodwar->getFrameCount());
						BWAPI::Position defensePoint = BWAPI::Position((BWAPI::Position(_infoManager.getMainPosition()).x + target->getPosition().x) / 2, (BWAPI::Position(_infoManager.getMainPosition()).y + target->getPosition().y) / 2);
						squad->attack(defensePoint, BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, BWAPI::Position(_infoManager.getMainPosition()).y), _flareBD);
					}
				}
			}
			else if (target->getPlayer() == BWAPI::Broodwar->neutral())
			{
				squad->setDefenseLastNeededFrame(BWAPI::Broodwar->getFrameCount());
				squad->attack(target, _flareBD);
			}
		}
	}

	// Check if we have/need any frontier squads to help defend out outer bases
	if (_infoManager.numFrontierSquadsNeeded() - numEmptyBases > 0 &&
		_infoManager.numFrontierSquadsNeeded() - numEmptyBases > _frontierSquads.size() && 
		(_infantrySquads.size() || _mechSquads.size()))
	{
		std::list< BWAPI::TilePosition> empty;

		BWAPI::TilePosition targetLocation = BuildingPlacer::Instance().getDesiredLocation(BWAPI::UnitTypes::Terran_Command_Center, _infoManager, empty);

		if (targetLocation.x < BWAPI::Broodwar->mapWidth() * .30) // left side of the map
			targetLocation += BWAPI::TilePosition(BWAPI::UnitTypes::Terran_Command_Center.tileWidth() * 2, 0);
		else
			targetLocation -= BWAPI::TilePosition(BWAPI::UnitTypes::Terran_Command_Center.tileWidth(), 0);

		// If we're getting more than 2 or three frontier squads its because the outer bases are running dry of resources.
		// To avoid taking too many units from the front, reassign the oldest squad to the newest target.
		if ((_infantrySquads.size() && _frontierSquads.size() >= 2) || (_mechSquads.size() && _frontierSquads.size() >= 3))
		{
			_frontierSquads.front().setFrontierLocation(targetLocation);
			_frontierSquads.push_back(_frontierSquads.front());
			_frontierSquads.pop_front();
			numEmptyBases += 1;
		}
		else
		{
			if (_infantrySquads.size())
			{
				bool found = false;

				for (std::list<Squad>::iterator & selection = _infantrySquads.begin(); selection != _infantrySquads.end(); selection++)
				{
					if (!selection->isGoodToAttack())
					{
						selection->setHaveGathered(true);
						_frontierSquads.push_back(*selection);
						_infantrySquads.erase(selection);

						found = true;

						_frontierSquads.back().setFrontierLocation(targetLocation);
						break;
					}
				}

				if (!found)
				{
					std::list<Squad>::iterator & selection = _infantrySquads.begin();
					selection->setHaveGathered(true);
					_frontierSquads.push_back(*selection);
					_infantrySquads.erase(selection);

					_frontierSquads.back().setFrontierLocation(targetLocation);
				}
			}
			else if (_mechSquads.size())
			{
				bool found = false;

				for (std::list<Squad>::iterator & selection = _mechSquads.begin(); selection != _mechSquads.end(); selection++)
				{
					if (!selection->isGoodToAttack())
					{
						selection->setHaveGathered(true);
						_frontierSquads.push_back(*selection);
						_mechSquads.erase(selection);

						found = true;

						_frontierSquads.back().setFrontierLocation(targetLocation);
						break;
					}
				}

				if (!found)
				{
					std::list<Squad>::iterator & selection = _mechSquads.begin();
					selection->setHaveGathered(true);
					_frontierSquads.push_back(*selection);
					_mechSquads.erase(selection);

					_frontierSquads.back().setFrontierLocation(targetLocation);
				}
			}
		}
	}

	if (_frontierSquads.size())
	{
		for (auto & squad : _frontierSquads)
		{
			squad.protect();
		}
	}

	// If we have squads, loop through to make sure they are where they need to be
	// Bio squads
	if (_infantrySquads.size())
	{
		for (auto & squad : _infantrySquads)
		{
			bool squadIsTooSpread = squad.tooSpreadOut();

			if ((_infoManager.getAggression() && squad.infantrySquadSize() == infantrySquadSizeLimit &&
				!squadIsTooSpread) || (squad.isMaxSupply() && squad.numMarines()))
			{
				squad.setGoodToAttack(true);
			}

			if (squad.isGoodToAttack())
			{
				BWAPI::Position forwardPosition = getForwardPoint(_infoManager);

				// initial attempt to group up squads so they're not a limbo line
				if (squadIsTooSpread && !squad.isMaxSupply())
				{
					bool haveWeGathered = squad.haveGatheredAtForwardPoint();
					squad.attack(squad.getSquadPosition(), squad.getSquadPosition(), _flareBD);
					squad.setHaveGathered(haveWeGathered);

				}
				else if (_infoManager.getEnemyBases().size())
				{
					if (targetIsDefended && !squad.isMaxSupply())
						squad.attack(forwardPosition, forwardPosition, _flareBD);
					else
						squad.attack(_infoManager.getEnemyBases().begin()->first, forwardPosition, _flareBD);
				}
				else if (_infoManager.getEnemyBuildingPositions().size())
				{
					squad.attack(_infoManager.getEnemyBuildingPositions().front(), forwardPosition, _flareBD);
				}
				else
				{
					squad.attack(BWAPI::Position(BWAPI::Position(nextUp).x, BWAPI::Position(nextUp).y), forwardPosition, _flareBD);
				}

				if (squad.numMarines() == 0)
				{
					squad.setGoodToAttack(false);
					squad.setHaveGathered(false);
				}
			}
			else
			{
				if (_infoManager.getBunkers().size())
				{
					if (_infoManager.getBunkers().size() == 1 && _infoManager.getOwnedBases().size() < 2 && !_infoManager.isExpanding())
					{
						squad.gather(_infoManager.getBunkers().front()->getPosition(), _flareBD);
					}
					else if ((_infoManager.isExpanding() && _infoManager.getOwnedBases().size() < 2 && _infoManager.getBunkers().size() < 2) ||
						(_infoManager.getBunkers().size() == 1 && !_infoManager.isExpanding()))
					{
						BWAPI::Position halfway = BWAPI::Position((BWAPI::Position(_infoManager.getNatPosition()).x + _infoManager.getNaturalChokePos().x) / 2, (BWAPI::Position(_infoManager.getNatPosition()).y + _infoManager.getNaturalChokePos().y) / 2);
						squad.gather(halfway, _flareBD);
					}
					else
					{
						BWAPI::Position point = _infoManager.getBunkers().front()->getPosition();
						int shortestDistance = _infoManager.getBunkers().front()->getDistance(_infoManager.getNaturalChokePos());
						for (auto bunker : _infoManager.getBunkers())
						{
							if (bunker->getDistance(_infoManager.getNaturalChokePos()) < shortestDistance)
							{
								point = bunker->getPosition();
								shortestDistance = bunker->getDistance(_infoManager.getNaturalChokePos());
							}
						}

						squad.gather(point, _flareBD);
					}
				}
				else
				{
					BWAPI::Position halfway = BWAPI::Position((BWAPI::Position(_infoManager.getNatPosition()).x + _infoManager.getNaturalChokePos().x) / 2, (BWAPI::Position(_infoManager.getNatPosition()).y + _infoManager.getNaturalChokePos().y) / 2);
					squad.gather(halfway, _flareBD);
				}
			}
		}

		// Erase empty squads
		for (std::list<Squad>::iterator emptySquad = _infantrySquads.begin(); emptySquad != _infantrySquads.end(); emptySquad++)
		{
			if (!emptySquad->infantrySquadSize())
			{
				_infantrySquads.erase(emptySquad);
				break;
			}
		}
	}
	// Mech squads
	else if (_mechSquads.size())
	{
		for (auto & squad : _mechSquads)
		{
			if ((_infoManager.getAggression() && squad.mechSquadSize() == mechSquadSizeLimit) || 400 - BWAPI::Broodwar->self()->supplyUsed() < 100)
			{
				squad.setGoodToAttack(true);
			}

			if (squad.isGoodToAttack())
			{
				BWAPI::Position forwardPosition = getForwardPoint(_infoManager);

				if (_infoManager.getEnemyBases().size())
				{
					squad.attack(_infoManager.getEnemyBases().begin()->first, forwardPosition, _flareBD);
				}
				else if (_infoManager.getEnemyBuildingPositions().size())
				{
					squad.attack(_infoManager.getEnemyBuildingPositions().front(), forwardPosition, _flareBD);
				}
				else
				{
					squad.attack(BWAPI::Position(BWAPI::Position(nextUp).x, BWAPI::Position(nextUp).y), forwardPosition, _flareBD);
				}

				if (squad.numTanks() == 0 && 400 - BWAPI::Broodwar->self()->supplyUsed() > 100)
				{
					squad.setGoodToAttack(false);
					squad.setHaveGathered(false);
				}
			}
			else
			{
				if (_infoManager.getBunkers().size())
				{
					if (_infoManager.getBunkers().size() == 1 && _infoManager.getOwnedBases().size() < 2 && !_infoManager.isExpanding())
					{
						squad.gather(_infoManager.getBunkers().front()->getPosition(), _flareBD);
					}
					else if ((_infoManager.isExpanding() && _infoManager.getOwnedBases().size() < 2 && _infoManager.getBunkers().size() < 2) ||
						(_infoManager.getBunkers().size() == 1 && !_infoManager.isExpanding()))
					{
						BWAPI::Position halfway = BWAPI::Position((BWAPI::Position(_infoManager.getNatPosition()).x + _infoManager.getNaturalChokePos().x) / 2, (BWAPI::Position(_infoManager.getNatPosition()).y + _infoManager.getNaturalChokePos().y) / 2);
						squad.gather(halfway, _flareBD);
					}
					else
					{
						BWAPI::Position point = _infoManager.getBunkers().front()->getPosition();
						int shortestDistance = _infoManager.getBunkers().front()->getDistance(_infoManager.getNaturalChokePos());
						for (auto bunker : _infoManager.getBunkers())
						{
							if (bunker->getDistance(_infoManager.getNaturalChokePos()) < shortestDistance)
							{
								point = bunker->getPosition();
								shortestDistance = bunker->getDistance(_infoManager.getNaturalChokePos());
							}
						}

						squad.gather(point, _flareBD);
					}
				}
				else
				{
					BWAPI::Position halfway = BWAPI::Position((BWAPI::Position(_infoManager.getNatPosition()).x + _infoManager.getNaturalChokePos().x) / 2, (BWAPI::Position(_infoManager.getNatPosition()).y + _infoManager.getNaturalChokePos().y) / 2);
					squad.gather(halfway, _flareBD);
				}
			}
		}

		// Erase empty squads
		for (std::list<Squad>::iterator emptySquad = _mechSquads.begin(); emptySquad != _mechSquads.end(); emptySquad++)
		{
			if (!emptySquad->mechSquadSize())
			{
				_mechSquads.erase(emptySquad);
				break;
			}
		}
	}

	// Our all encompassing All In Squad
	if (_allInSquad.size())
	{
		if (_infoManager.getStrategy() == "MechAllIn" || _infoManager.isTwoBasePlay(_infoManager.getStrategy()))
		{
			for (auto & squad : _allInSquad)
			{
				if ((!squad.isGoodToAttack() && _infoManager.getAggression()) ||
					BWAPI::Broodwar->getFrameCount() >= 12000)
				{
					squad.setGoodToAttack(true);
					squad.setHaveGathered(true);
				}

				if (squad.isGoodToAttack())
				{
					BWAPI::Position forwardPosition = getForwardPoint(_infoManager);

					if (_infoManager.getEnemyBases().size())
					{
						squad.attack(_infoManager.getEnemyBases().begin()->first, forwardPosition, _flareBD);
					}
					else if (_infoManager.getEnemyBuildingPositions().size())
					{
						squad.attack(_infoManager.getEnemyBuildingPositions().front(), forwardPosition, _flareBD);
					}
					else
					{
						squad.attack(BWAPI::Position(BWAPI::Position(nextUp).x, BWAPI::Position(nextUp).y), forwardPosition, _flareBD);
					}
				}
				else
				{
					if (_infoManager.getBunkers().size())
						squad.gather(_infoManager.getBunkers().front()->getPosition(), _flareBD);
					else
						squad.gather(BWAPI::Position(_infoManager.getMainPosition()), _flareBD);
				}
			}
		}
		else
		{
			for (auto & squad : _allInSquad)
			{
				if (!squad.isGoodToAttack() && _infoManager.getAggression() && squad.numTanks() > 0 && squad.numMarines() > 6)
				{
					squad.setGoodToAttack(true);
					squad.setHaveGathered(true);
				}

				if (squad.isGoodToAttack())
				{
					BWAPI::Position forwardPosition = getForwardPoint(_infoManager);

					if (_infoManager.getEnemyBases().size())
					{
						squad.attack(_infoManager.getEnemyBases().begin()->first, forwardPosition, _flareBD);
					}
					else if (_infoManager.getEnemyBuildingPositions().size())
					{
						squad.attack(_infoManager.getEnemyBuildingPositions().front(), forwardPosition, _flareBD);
					}
					else
					{
						squad.attack(BWAPI::Position(BWAPI::Position(nextUp).x, BWAPI::Position(nextUp).y), forwardPosition, _flareBD);
					}

					if (squad.numTanks() == 0 && 400 - BWAPI::Broodwar->self()->supplyUsed() > 100)
					{
						squad.setGoodToAttack(false);
						squad.setHaveGathered(false);
					}
				}
				else
				{
					if (_infoManager.getBunkers().size())
						squad.gather(_infoManager.getBunkers().front()->getPosition(), _flareBD);
					else
						squad.gather(_infoManager.getMainChokePos(), _flareBD);
				}
			}
		}
		

		// Erase empty squads
		for (std::list<Squad>::iterator emptySquad = _specialistSquad.begin(); emptySquad != _specialistSquad.end(); emptySquad++)
		{
			if (!emptySquad->specialistSquadSize())
			{
				_specialistSquad.erase(emptySquad);
				break;
			}
		}
	}

	//Specialist Squad
	if (_specialistSquad.size())
	{
		bool shouldMoveForward = false;

		if (targetIsDefended)
		{
			for (auto squad : _infantrySquads)
			{
				if (_infoManager.closeEnough(squad.getSquadPosition(), getForwardPoint(_infoManager)))
				{
					shouldMoveForward = true;
				}
			}
		}

		for (auto & squad : _specialistSquad)
		{
			if (_infoManager.getAggression() && targetIsDefended && shouldMoveForward)
			{
				squad.setGoodToAttack(true);
			}
			else if (squad.isGoodToAttack() && !shouldMoveForward)
			{
				squad.setGoodToAttack(false);
			}

			if (squad.isGoodToAttack())
			{
				BWAPI::Position forwardPosition = getGhostForwardPoint(_infoManager);

				if (_infoManager.getEnemyBases().size())
				{
					if (targetIsDefended && !squad.isMaxSupply())
					{
						if (_infoManager.getNumFinishedUnit(BWAPI::UnitTypes::Terran_Nuclear_Missile) && !squad.getNuker())
						{
							// Set up to nuke the enemy fortified position
							squad.setNuker();
						}

						if (squad.getNuker() && squad.getNuker()->exists())
						{
							BWAPI::Position target = _infoManager.getEnemyBases().begin()->first;

							if (_infoManager.closeEnough(target, BWAPI::Position(_infoManager.getEnemyMainTilePos())))
							{
								for (auto enemyBase : _infoManager.getEnemyBases())
								{
									if (_infoManager.closeEnough(enemyBase.first, _infoManager.getEnemyNaturalPos()))
									{
										target = enemyBase.first;
										break;
									}
								}
							}

							squad.handleNuker(target);
						}

						squad.attack(forwardPosition, forwardPosition, _flareBD);
					}
					else
					{
						if (squad.getNuker())
							squad.clearNuker();

						squad.attack(_infoManager.getEnemyBases().begin()->first, forwardPosition, _flareBD);
					}
				}
				else if (_infoManager.getEnemyBuildingPositions().size())
				{
					squad.attack(_infoManager.getEnemyBuildingPositions().front(), forwardPosition, _flareBD);
				}
				else
				{
					squad.attack(BWAPI::Position(BWAPI::Position(nextUp).x, BWAPI::Position(nextUp).y), forwardPosition, _flareBD);
				}

				if (squad.numGhosts() == 0)
				{
					squad.setGoodToAttack(false);
					squad.setHaveGathered(false);
				}
			}
			else
			{
				if (_infoManager.getBunkers().size())
				{
					if (_infoManager.getBunkers().size() == 1 && _infoManager.getOwnedBases().size() < 2 && !_infoManager.isExpanding())
					{
						squad.gather(_infoManager.getBunkers().front()->getPosition(), _flareBD);
					}
					else if ((_infoManager.isExpanding() && _infoManager.getOwnedBases().size() < 2 && _infoManager.getBunkers().size() < 2) ||
						(_infoManager.getBunkers().size() == 1 && !_infoManager.isExpanding()))
					{
						BWAPI::Position halfway = BWAPI::Position((BWAPI::Position(_infoManager.getNatPosition()).x + _infoManager.getNaturalChokePos().x) / 2, (BWAPI::Position(_infoManager.getNatPosition()).y + _infoManager.getNaturalChokePos().y) / 2);
						squad.gather(halfway, _flareBD);
					}
					else
					{
						BWAPI::Position point = _infoManager.getBunkers().front()->getPosition();
						int shortestDistance = _infoManager.getBunkers().front()->getDistance(_infoManager.getNaturalChokePos());
						for (auto bunker : _infoManager.getBunkers())
						{
							if (bunker->getDistance(_infoManager.getNaturalChokePos()) < shortestDistance)
							{
								point = bunker->getPosition();
								shortestDistance = bunker->getDistance(_infoManager.getNaturalChokePos());
							}
						}

						squad.gather(point, _flareBD);
					}
				}
				else
				{
					BWAPI::Position halfway = BWAPI::Position((BWAPI::Position(_infoManager.getNatPosition()).x + _infoManager.getNaturalChokePos().x) / 2, (BWAPI::Position(_infoManager.getNatPosition()).y + _infoManager.getNaturalChokePos().y) / 2);
					squad.gather(halfway, _flareBD);
				}
			}
		}

		// Erase empty squads
		for (std::list<Squad>::iterator emptySquad = _specialistSquad.begin(); emptySquad != _specialistSquad.end(); emptySquad++)
		{
			if (!emptySquad->specialistSquadSize())
			{
				_specialistSquad.erase(emptySquad);
				break;
			}
		}
	}

	if (_BCsquad.size())
	{
		for (auto & squad : _BCsquad)
		{
			if (!squad.isGoodToAttack() && _infoManager.getAggression() && squad.numBCs() == 4)
			{
				squad.setGoodToAttack(true);
			}

			if (squad.isGoodToAttack())
			{
				BWAPI::Position forwardPosition = _infoManager.getAirGatherLocation();

				if (_infoManager.getEnemyBases().size())
				{
					squad.attack(_infoManager.getEnemyBases().begin()->first, forwardPosition, _flareBD);
				}
				else if (_infoManager.getEnemyBuildingPositions().size())
				{
					squad.attack(_infoManager.getEnemyBuildingPositions().front(), forwardPosition, _flareBD);
				}
				else
				{
					squad.attack(BWAPI::Position(BWAPI::Position(nextUp).x, BWAPI::Position(nextUp).y), forwardPosition, _flareBD);
				}
			}
			else
			{
				squad.gather(BWAPI::Position(_infoManager.getMainPosition()), _flareBD);
			}
		}


		// Erase empty squads
		for (std::list<Squad>::iterator emptySquad = _specialistSquad.begin(); emptySquad != _specialistSquad.end(); emptySquad++)
		{
			if (!emptySquad->specialistSquadSize())
			{
				_specialistSquad.erase(emptySquad);
				break;
			}
		}
	}

	// Dropship
	if (_infoManager.getDropships().size())
	{
		BWAPI::Broodwar->setLocalSpeed(10);
		handleDropships(_infoManager);
	}

	if (_dropSquad.size() && dropping)
	{
		for (auto & squad : _dropSquad)
		{
			squad.drop(BWAPI::Broodwar->enemy()->getUnits());
		}
	}

	/*****************************************************************
	* Spellcasting section
	******************************************************************/
	// Vessel handling
	for (auto & vessel : _infoManager.getVessels())
	{
		if (!vessel.first->exists())
		{
			continue;
		}

		for (std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>::iterator target = _irradiateDB.begin(); target != _irradiateDB.end();)
		{
			if (BWAPI::Broodwar->getFrameCount() - target->second.second > 72)
			{
				target = _irradiateDB.erase(target);
				continue;
			}

			if (target->first == vessel.first)
			{
				if (vessel.first->getEnergy() > 75 )//&& vessel.first->getOrder() != BWAPI::Orders::CastIrradiate)
				{
					vessel.first->useTech(BWAPI::TechTypes::Irradiate, target->second.first);
					goto nextVessel;
				}
			}

			target++;
		}

		for (std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>::iterator defensiveTarget = _MatrixDB.begin(); defensiveTarget != _MatrixDB.end();)
		{
			if (BWAPI::Broodwar->getFrameCount() - defensiveTarget->second.second > 72)
			{
				defensiveTarget = _MatrixDB.erase(defensiveTarget);
				continue;
			}

			if (defensiveTarget->first == vessel.first)
			{
				if (vessel.first->getEnergy() > 100)
				{
					vessel.first->useTech(BWAPI::TechTypes::Defensive_Matrix, defensiveTarget->second.first);
					goto nextVessel;
				}
			}

			defensiveTarget++;
		}

		if (!irradiateTarget(vessel.first) && !matrixTarget(vessel.first))
		{
			BWAPI::Position closest = _infoManager.getNaturalChokePos();
			int distance = 99999;
			std::list<Squad> _squads;
			if (stratIsBio)
			{
				_squads = _infantrySquads;
			}
			else if (stratIsMech)
			{
				_squads = _mechSquads;
			}
			else if (stratIsAllIn)
			{
				_squads = _allInSquad;
			}

			// Shouldn't matter too much, but don't send vessels to the nat if we have no nat
			if (_infoManager.getOwnedBases().size() < 2)
			{
				if (_infoManager.getBunkers().size())
				{
					closest = _infoManager.getBunkers().front()->getPosition();
				}
				else if (_infoManager.getCommandCenters().size())
				{
					closest = _infoManager.getCommandCenters().front()->getPosition();
				}
			}

			if (_infoManager.getEnemyBases().size())
			{
				if (stratIsAllIn)
				{
					BWAPI::Unit closestTank = NULL;

					for (auto squad : _squads)
					{
						for (auto tank : squad.getTanks())
						{
							if (tank.first->getDistance(_infoManager.getEnemyBases().begin()->first) < distance)
							{
								distance = tank.first->getDistance(_infoManager.getEnemyBases().begin()->first);
								closestTank = tank.first;
							}
						}
					}

					if (closestTank)
						closest = closestTank->getPosition();
				}
				else
				{
					for (auto squad : _squads)
					{
						if (sqrt(((squad.getSquadPosition().x - _infoManager.getEnemyBases().begin()->first.x) * (squad.getSquadPosition().x - _infoManager.getEnemyBases().begin()->first.x)) +
							((squad.getSquadPosition().y - _infoManager.getEnemyBases().begin()->first.y)) * (squad.getSquadPosition().y - _infoManager.getEnemyBases().begin()->first.y)) < distance &&
							squad.isGoodToAttack())
						{
							distance = sqrt(((squad.getSquadPosition().x - _infoManager.getEnemyBases().begin()->first.x) * (squad.getSquadPosition().x - _infoManager.getEnemyBases().begin()->first.x)) +
								((squad.getSquadPosition().y - _infoManager.getEnemyBases().begin()->first.y)) * (squad.getSquadPosition().y - _infoManager.getEnemyBases().begin()->first.y));
							closest = squad.getSquadPosition();
						}
					}
				}
				
				if (abs(vessel.first->getPosition().x - closest.x) <= 64 && abs(vessel.first->getPosition().y - closest.y) <= 64)
				{
					continue;
				}
				else
				{
					vessel.first->move(closest);
				}
			}
			else if (_infoManager.getEnemyBuildingPositions().size())
			{
				if (stratIsAllIn)
				{
					BWAPI::Unit closestTank = NULL;

					for (auto squad : _squads)
					{
						for (auto tank : squad.getTanks())
						{
							if (tank.first->getDistance(_infoManager.getEnemyBuildingPositions().front()) < distance)
							{
								distance = tank.first->getDistance(_infoManager.getEnemyBuildingPositions().front());
								closestTank = tank.first;
							}
						}
					}

					if (closestTank)
						closest = closestTank->getPosition();
				}
				else
				{
					for (auto squad : _squads)
					{
						if (sqrt(((squad.getSquadPosition().x - _infoManager.getEnemyBuildingPositions().front().x) * (squad.getSquadPosition().x - _infoManager.getEnemyBuildingPositions().front().x)) +
							((squad.getSquadPosition().y - _infoManager.getEnemyBuildingPositions().front().y)) * (squad.getSquadPosition().y - _infoManager.getEnemyBuildingPositions().front().y)) < distance &&
							squad.isGoodToAttack())
						{
							distance = sqrt(((squad.getSquadPosition().x - _infoManager.getEnemyBuildingPositions().front().x) * (squad.getSquadPosition().x - _infoManager.getEnemyBuildingPositions().front().x)) +
								((squad.getSquadPosition().y - _infoManager.getEnemyBuildingPositions().front().y)) * (squad.getSquadPosition().y - _infoManager.getEnemyBuildingPositions().front().y));
							closest = squad.getSquadPosition();
						}
					}
				}

				if (abs(vessel.first->getPosition().x - closest.x) <= 64 && abs(vessel.first->getPosition().y - closest.y) <= 64)
				{
					continue;
				}
				else
				{
					vessel.first->move(closest);
				}
			}
			else
			{
				if (stratIsAllIn)
				{
					BWAPI::Unit closestTank = NULL;

					for (auto squad : _squads)
					{
						for (auto tank : squad.getTanks())
						{
							if (tank.first->getDistance(BWAPI::Position(nextUp)) < distance)
							{
								distance = tank.first->getDistance(BWAPI::Position(nextUp));
								closestTank = tank.first;
							}
						}
					}

					if (closestTank)
						closest = closestTank->getPosition();
				}
				else
				{
					for (auto squad : _squads)
					{
						if (sqrt(((squad.getSquadPosition().x - BWAPI::Position(nextUp).x) * (squad.getSquadPosition().x - BWAPI::Position(nextUp).x)) +
							((squad.getSquadPosition().y - BWAPI::Position(nextUp).y)) * (squad.getSquadPosition().y - BWAPI::Position(nextUp).y)) < distance &&
							squad.isGoodToAttack())
						{
							distance = sqrt(((squad.getSquadPosition().x - BWAPI::Position(nextUp).x) * (squad.getSquadPosition().x - BWAPI::Position(nextUp).x)) +
								((squad.getSquadPosition().y - BWAPI::Position(nextUp).y)) * (squad.getSquadPosition().y - BWAPI::Position(nextUp).y));
							closest = squad.getSquadPosition();
						}
					}
				}

				if (abs(vessel.first->getPosition().x - closest.x) <= 64 && abs(vessel.first->getPosition().y - closest.y) <= 64)
				{
					continue;
				}
				else
				{
					vessel.first->move(closest);
				}
			}
		}

	nextVessel: continue;
	}
}

// Where are we sending the squads?
BWAPI::Position UnitManager::getForwardPoint(InformationManager & _infoManager)
{
	BWAPI::TilePosition center = BWAPI::TilePosition(BWAPI::TilePosition(BWAPI::Broodwar->mapWidth() / 2, BWAPI::Broodwar->mapHeight() / 2));

	BWAPI::Position forwardPosition = BWAPI::Position(0, 0);

	if (!_infoManager.getEnemyBases().size())
		return forwardPosition;

	bool targetIsMain = _infoManager.closeEnough(_infoManager.getEnemyBases().begin()->first, BWAPI::Position(_infoManager.getEnemyMainTilePos()));
	bool targetIsNat = _infoManager.closeEnough(_infoManager.getEnemyNaturalPos(), _infoManager.getEnemyBases().begin()->first);
	bool enemyHasNat = false;

	if (targetIsMain)
	{
		for (auto enemyBase : _infoManager.getEnemyBases())
		{
			if (_infoManager.closeEnough(_infoManager.getEnemyNaturalPos(), enemyBase.first))
			{
				enemyHasNat = true;
			}
		}
	}

	if ((targetIsMain && enemyHasNat) || targetIsNat)
	{
		forwardPosition = BWAPI::Position(_infoManager.getEnemyNatChokePos().x + (BWAPI::Position(center).x - _infoManager.getEnemyNatChokePos().x) * .25,
			_infoManager.getEnemyNatChokePos().y + (BWAPI::Position(center).y - _infoManager.getEnemyNatChokePos().y) * .25);
	}
	else if (targetIsMain)
	{
		forwardPosition = _infoManager.getEnemyNaturalPos();
	}
	else
	{
		forwardPosition = BWAPI::Position(_infoManager.getEnemyBases().begin()->first.x + (BWAPI::Position(center).x - _infoManager.getEnemyBases().begin()->first.x) * .35,
			_infoManager.getEnemyBases().begin()->first.y + (BWAPI::Position(center).y - _infoManager.getEnemyBases().begin()->first.y) * .35);
	}

	return forwardPosition;
}

// We want the specialists to be a bit further back
BWAPI::Position UnitManager::getGhostForwardPoint(InformationManager & _infoManager)
{
	BWAPI::TilePosition center = BWAPI::TilePosition(BWAPI::TilePosition(BWAPI::Broodwar->mapWidth() / 2, BWAPI::Broodwar->mapHeight() / 2));

	BWAPI::Position forwardPosition = BWAPI::Position(0, 0);

	if (!_infoManager.getEnemyBases().size())
		return forwardPosition;

	bool targetIsMain = _infoManager.closeEnough(_infoManager.getEnemyBases().begin()->first, BWAPI::Position(_infoManager.getEnemyMainTilePos()));
	bool targetIsNat = _infoManager.closeEnough(_infoManager.getEnemyNaturalPos(), _infoManager.getEnemyBases().begin()->first);
	bool enemyHasNat = false;

	if (targetIsMain)
	{
		for (auto enemyBase : _infoManager.getEnemyBases())
		{
			if (_infoManager.closeEnough(_infoManager.getEnemyNaturalPos(), enemyBase.first))
			{
				enemyHasNat = true;
			}
		}
	}

	if ((targetIsMain && enemyHasNat) || targetIsNat)
	{
		forwardPosition = BWAPI::Position(_infoManager.getEnemyNatChokePos().x + (BWAPI::Position(center).x - _infoManager.getEnemyNatChokePos().x) * .35,
			_infoManager.getEnemyNatChokePos().y + (BWAPI::Position(center).y - _infoManager.getEnemyNatChokePos().y) * .35);
	}
	else if (targetIsMain)
	{
		forwardPosition = BWAPI::Position(_infoManager.getEnemyNatChokePos().x + (BWAPI::Position(center).x - _infoManager.getEnemyNatChokePos().x) * .20,
			_infoManager.getEnemyNatChokePos().y + (BWAPI::Position(center).y - _infoManager.getEnemyNatChokePos().y) * .20);
	}
	else
	{
		forwardPosition = BWAPI::Position(_infoManager.getEnemyBases().begin()->first.x + (BWAPI::Position(center).x - _infoManager.getEnemyBases().begin()->first.x) * .40,
			_infoManager.getEnemyBases().begin()->first.y + (BWAPI::Position(center).y - _infoManager.getEnemyBases().begin()->first.y) * .40);
	}

	return forwardPosition;
}

void UnitManager::handleScout(BWAPI::Unit & _scout, const BWAPI::Position enemyMain)
{
	if (!_scout || !_scout->exists())
		return;

	if (enemyMain != BWAPI::Position(0, 0) && circlePositions.empty())
	{
		int numPoints = 36;
		int radius = 400; // or whatever radius you want

		for (int i = 0; i < numPoints; ++i) {
			double angle = i * 2 * 3.14159265358979323846 / numPoints;
			int x = static_cast<int>(enemyMain.x + radius * cos(angle));
			int y = static_cast<int>(enemyMain.y + radius * sin(angle));

			BWAPI::TilePosition edge = BWAPI::TilePosition(BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

			if (x < 0)
				x = 10;
			if (x >= BWAPI::Position(edge).x)
				x = BWAPI::Position(edge).x - 10;
			if (y < 0)
				y = 10;
			if (y >= BWAPI::Position(edge).y)
				y = BWAPI::Position(edge).y - 10;

			circlePositions.push_back(BWAPI::Position(x, y));
		}
	}

	if (enemyMain == BWAPI::Position(0,0) &&
		(!_scout->isMoving() || _scout->isGatheringMinerals() || 
		_scout->isIdle() || BWAPI::Broodwar->isExplored(scoutNextTarget)))
	{
		for (auto location : theMap.StartingLocations())
		{
			if (!BWAPI::Broodwar->isExplored(location))
			{
				_scout->move(BWAPI::Position(location));
				scoutNextTarget = location;
				break;
			}
		}
	}
	else if (enemyMain != BWAPI::Position(0, 0) && !circlePositions.empty())
	{
		if (_scout->getDistance(circlePositions.front()) < 64 || 
			!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(circlePositions.front())) ||
			!BWAPI::Broodwar->getUnitsInRectangle(circlePositions.front(), circlePositions.front() + BWAPI::Position(1,1), BWAPI::Filter::IsBuilding).empty())
		{
			circlePositions.push_back(circlePositions.front());
			circlePositions.pop_front();
		}
		else
		{
			_scout->move(circlePositions.front());
		}
	}
}

bool insanitybot::UnitManager::assignSquad(BWAPI::Unit unassigned, bool bio, bool allIn)
{
	if (!unassigned || !unassigned->exists())
		return false;

	if (bio)
	{
		if (_defensiveSquads.size() && ((unassigned->getType() == BWAPI::UnitTypes::Terran_Marine && _defensiveSquads.front().numMarines() < 12) ||
			(unassigned->getType() == BWAPI::UnitTypes::Terran_Medic && _defensiveSquads.front().numMedics() < 3)))
		{
			for (auto & squad : _defensiveSquads)
			{
				if (unassigned->getType() == BWAPI::UnitTypes::Terran_Marine && squad.numMarines() == 12)
					continue;

				if (unassigned->getType() == BWAPI::UnitTypes::Terran_Medic && squad.numMedics() == 3)
					continue;

				if (unassigned->getType() == BWAPI::UnitTypes::Terran_Marine)
				{
					squad.addMarine(unassigned);
					return true;
				}
				else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Medic)
				{
					squad.addMedic(unassigned);
					return true;
				}
			}
		}
		else if (_dropSquad.size() && _dropSquad.front().dropSquadSize() < dropSquadSizeLimit &&
				((unassigned->getType() == BWAPI::UnitTypes::Terran_Marine && _dropSquad.front().numMarines() < 7) ||
				(unassigned->getType() == BWAPI::UnitTypes::Terran_Medic && _dropSquad.front().numMedics() < 1)) &&
				!dropping)
		{
			for (auto & squad : _dropSquad)
			{
				if (squad.dropSquadSize() == dropSquadSizeLimit)
					continue;

				if (unassigned->getType() == BWAPI::UnitTypes::Terran_Marine && squad.numMarines() >= 7)
					continue;

				if (unassigned->getType() == BWAPI::UnitTypes::Terran_Medic && squad.numMedics() >= 1)
					continue;

				if (unassigned->getType() == BWAPI::UnitTypes::Terran_Marine)
				{
					squad.addMarine(unassigned);
					return true;
				}
				else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Medic)
				{
					squad.addMedic(unassigned);
					return true;
				}
			}
		}
		else if (_infantrySquads.size())
		{
			if (_frontierSquads.size())
			{
				for (auto & squad : _frontierSquads)
				{
					if (squad.frontierSquadSize() == frontierSquadSizeLimit)
						continue;

					if (unassigned->getType() == BWAPI::UnitTypes::Terran_Marine && squad.numMarines() >= 8)
						continue;

					if (unassigned->getType() == BWAPI::UnitTypes::Terran_Medic && squad.numMedics() >= 2)
						continue;

					if (unassigned->getType() == BWAPI::UnitTypes::Terran_Marine)
					{
						squad.addMarine(unassigned);
						return true;
					}
					else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Medic)
					{
						squad.addMedic(unassigned);
						return true;
					}
				}
			}

			for (auto & squad : _infantrySquads)
			{
				if (squad.infantrySquadSize() == infantrySquadSizeLimit || (squad.isGoodToAttack() && !squad.isMaxSupply()))
					continue;

				if (unassigned->getType() == BWAPI::UnitTypes::Terran_Marine && squad.numMarines() == 12)
					continue;

				if (unassigned->getType() == BWAPI::UnitTypes::Terran_Medic && squad.numMedics() == 3)
					continue;

				if (unassigned->getType() == BWAPI::UnitTypes::Terran_Marine)
				{
					squad.addMarine(unassigned);
					return true;
				}
				else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Medic)
				{
					squad.addMedic(unassigned);
					return true;
				}
			}

			if (_infantrySquads.size() < 12)
			{
				_infantrySquads.push_back(Squad(unassigned, false));
				return true;
			}
		}
		else
		{
			_infantrySquads.push_back(Squad(unassigned, false));
			return true;
		}
	}
	else if (allIn)
	{
		if (_defensiveSquads.size() && ((unassigned->getType() == BWAPI::UnitTypes::Terran_Marine && _defensiveSquads.front().numMarines() < 12) ||
			(unassigned->getType() == BWAPI::UnitTypes::Terran_Medic && _defensiveSquads.front().numMedics() < 3)))
		{
			for (auto & squad : _defensiveSquads)
			{
				if (unassigned->getType() == BWAPI::UnitTypes::Terran_Marine && squad.numMarines() == 12)
					continue;

				if (unassigned->getType() == BWAPI::UnitTypes::Terran_Medic && squad.numMedics() == 3)
					continue;

				if (unassigned->getType() == BWAPI::UnitTypes::Terran_Marine)
				{
					squad.addMarine(unassigned);
					return true;
				}
				else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Medic)
				{
					squad.addMedic(unassigned);
					return true;
				}
			}
		}
		else if (_allInSquad.size())
		{
			for (auto & squad : _allInSquad)
			{
				if (unassigned->getType() == BWAPI::UnitTypes::Terran_Marine)
				{
					squad.addMarine(unassigned);
					return true;
				}
				else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Medic)
				{
					squad.addMedic(unassigned);
					return true;
				}
				else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Vulture)
				{
					squad.addVulture(unassigned);
					return true;
				}
				else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
				{
					squad.addTank(unassigned);
					return true;
				}
				else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Goliath)
				{
					squad.addGoliath(unassigned);
					return true;
				}
			}
		}
		else
		{
			_allInSquad.push_back(Squad(unassigned, true));
			return true;
		}
	}
	else
	{
		if (unassigned->getType() == BWAPI::UnitTypes::Terran_Ghost)
		{
			if (_specialistSquad.size())
			{
				for (auto & squad : _specialistSquad)
				{
					if (squad.specialistSquadSize() == specialistSquadSizeLimit)
						continue;
					
					squad.addGhost(unassigned);
					return true;
				}
			}
			else
			{
				_specialistSquad.push_back(Squad(unassigned, false));
				return true;
			}
		}
		else
		{
			if (_defensiveSquads.size() && ((unassigned->getType() == BWAPI::UnitTypes::Terran_Vulture && _defensiveSquads.front().numVultures() + _defensiveSquads.front().numGoliaths() < 4) ||
				(unassigned->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode && _defensiveSquads.front().numTanks() < 3) ||
				(unassigned->getType() == BWAPI::UnitTypes::Terran_Goliath && _defensiveSquads.front().numVultures() + _defensiveSquads.front().numGoliaths() < 6)))
			{
				for (auto & squad : _defensiveSquads)
				{
					if (unassigned->getType() == BWAPI::UnitTypes::Terran_Vulture && squad.numVultures() + squad.numGoliaths() >= 6)
						continue;

					if (unassigned->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode && squad.numTanks() >= 3)
						continue;

					if (unassigned->getType() == BWAPI::UnitTypes::Terran_Goliath && squad.numVultures() + squad.numGoliaths() >= 6)
						continue;

					if (unassigned->getType() == BWAPI::UnitTypes::Terran_Vulture)
					{
						squad.addVulture(unassigned);
						return true;
					}
					else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
					{
						squad.addTank(unassigned);
						return true;
					}
					else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Goliath)
					{
						squad.addGoliath(unassigned);
						return true;
					}
				}
			}
			else if (_dropSquad.size() && _dropSquad.front().dropSquadSize() < dropSquadSizeLimit &&											// We have a squad
				(((unassigned->getType() == BWAPI::UnitTypes::Terran_Vulture || unassigned->getType() == BWAPI::UnitTypes::Terran_Goliath) &&	// It has free space
					_dropSquad.front().numVultures() + _dropSquad.front().numGoliaths() < 2) ||
				(unassigned->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode && _dropSquad.front().numTanks() < 1)) &&
				!dropping)																														// We are not in the process of dropping of the units across the map
			{
				for (auto & squad : _dropSquad)
				{
					if (squad.dropSquadSize() == dropSquadSizeLimit)
						continue;

					if ((unassigned->getType() == BWAPI::UnitTypes::Terran_Vulture || unassigned->getType() == BWAPI::UnitTypes::Terran_Goliath) && 
						squad.numVultures() + squad.numGoliaths() >= 2)
						continue;

					if (unassigned->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode && squad.numTanks() >= 1)
						continue;

					if (unassigned->getType() == BWAPI::UnitTypes::Terran_Vulture)
					{
						squad.addVulture(unassigned);
						return true;
					}
					else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Goliath)
					{
						squad.addGoliath(unassigned);
						return true;
					}
					else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
					{
						squad.addTank(unassigned);
						return true;
					}
				}
			}
			else if (_mechSquads.size())
			{
				if (_frontierSquads.size())
				{
					for (auto & squad : _frontierSquads)
					{
						if (squad.frontierSquadSize() == frontierSquadSizeLimit)
							continue;

						if (unassigned->getType() == BWAPI::UnitTypes::Terran_Vulture && squad.numVultures() + squad.numGoliaths() >= 3)
							continue;

						if (unassigned->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode && squad.numTanks() >= 2)
							continue;

						if (unassigned->getType() == BWAPI::UnitTypes::Terran_Goliath && squad.numVultures() + squad.numGoliaths() >= 3)
							continue;

						if (unassigned->getType() == BWAPI::UnitTypes::Terran_Vulture)
						{
							squad.addVulture(unassigned);
							return true;
						}
						else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
						{
							squad.addTank(unassigned);
							return true;
						}
						else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Goliath)
						{
							squad.addGoliath(unassigned);
							return true;
						}
					}
				}

				for (auto & squad : _mechSquads)
				{
					if ((squad.mechSquadSize() == mechSquadSizeLimit || squad.isGoodToAttack()) && 400 - BWAPI::Broodwar->self()->supplyUsed() > 100)
						continue;

					if (unassigned->getType() == BWAPI::UnitTypes::Terran_Vulture && squad.numVultures() + squad.numGoliaths() >= 6)
						continue;

					if (unassigned->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode && squad.numTanks() >= 3)
						continue;

					if (unassigned->getType() == BWAPI::UnitTypes::Terran_Goliath && squad.numVultures() + squad.numGoliaths() >= 6)
						continue;

					if (unassigned->getType() == BWAPI::UnitTypes::Terran_Vulture)
					{
						squad.addVulture(unassigned);
						return true;
					}
					else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
					{
						squad.addTank(unassigned);
						return true;
					}
					else if (unassigned->getType() == BWAPI::UnitTypes::Terran_Goliath)
					{
						squad.addGoliath(unassigned);
						return true;
					}
				}

				if (_mechSquads.size() < 10)
				{
					_mechSquads.push_back(Squad(unassigned, false));
					return true;
				}
			}
			else
			{
				_mechSquads.push_back(Squad(unassigned, false));
				return true;
			}
		}
	}

	return false;
}

bool insanitybot::UnitManager::assignAirSquad(BWAPI::Unit unassigned)
{
	for (auto & squad : _BCsquad)
	{
		if (unassigned->getType() == BWAPI::UnitTypes::Terran_Battlecruiser && squad.numBCs() >= 4)
			continue;

		if (unassigned->getType() == BWAPI::UnitTypes::Terran_Battlecruiser)
		{
			squad.addBC(unassigned);
			return true;
		}
	}

	_BCsquad.push_back(Squad(unassigned, false));
	return true;
}
/*****************************************************
* Handle bunker and squad assignments
******************************************************/
void UnitManager::assignBio(InformationManager & _infoManager)
{
	// Loop through our marines and make sure they are assigned to a squad
	for (auto & marine : _infoManager.getMarines())
	{
		bool loading = false;

		if (!marine.first->exists())
		{
			continue;
		}

		if (_infoManager.getBunkers().size() && marine.second == 0)
		{
			for (auto bunker : _infoManager.getBunkers())
			{
				if (bunker->getLoadedUnits().size() < 4 && !bunker->isConstructing())
				{
					marine.first->load(bunker);
					loading = true;
					break;
				}
				else if (bunker->isConstructing())
				{
					if (!_infoManager.closeEnough(bunker->getPosition(), marine.first->getPosition()))
						marine.first->move(bunker->getPosition());

					loading = true;
					break;
				}
			}
		}

		if (marine.first->isLoaded() || loading) continue;

		if (marine.second == 0)
		{
			if (assignSquad(marine.first, true, false))
				marine.second = 1;
		}
	}

	// Loop through our medics and make sure they are assigned to a squad
	for (auto & medic : _infoManager.getMedics())
	{
		if (!medic.first->exists())
		{
			continue;
		}

		if (medic.second == 0)
		{
			if (assignSquad(medic.first, true, false))
				medic.second = 1;
		}
	}
}

void UnitManager::assignMech(InformationManager & _infoManager)
{
	//Load marines into bunkers
	for (auto & marine : _infoManager.getMarines())
	{
		if (!marine.first->exists())
		{
			continue;
		}

		if (_infoManager.getBunkers().size() && marine.second == 0)
		{
			for (auto bunker : _infoManager.getBunkers())
			{
				if (bunker->isCompleted() && bunker->getLoadedUnits().size() < 4)
				{
					marine.first->load(bunker);
				}
				else
				{
					if (marine.first->getDistance(bunker) > 50)
						marine.first->move(bunker->getPosition());
				}
			}
		}
	}
	// Loop through our Vultures and make sure they are assigned to a squad
	for (auto & vulture : _infoManager.getVultures())
	{
		if (!vulture.first->exists())
		{
			continue;
		}

		if (vulture.second == 0)
		{
			if (assignSquad(vulture.first, false, false))
				vulture.second = 1;
		}
	}

	// Loop through our tanks and make sure they are assigned to a squad
	for (auto & tank : _infoManager.getTanks())
	{
		if (!tank.first->exists())
		{
			continue;
		}

		if (tank.second == 0)
		{
			if (assignSquad(tank.first, false, false))
				tank.second = 1;
		}
	}

	// Loop through our Goliaths and make sure they are assigned to a squad
	for (auto & goliath : _infoManager.getGoliaths())
	{
		if (!goliath.first || !goliath.first->exists())
		{
			continue;
		}

		if (goliath.second == 0)
		{
			if (assignSquad(goliath.first, false, false))
			{
				goliath.second = 1;
			}
		}
	}
}

void UnitManager::assignAllIn(InformationManager & _infoManager)
{
	//Everyone is in here
	// Loop through our marines and make sure they are assigned to a squad
	for (auto & marine : _infoManager.getMarines())
	{
		bool loading = false;

		if (!marine.first->exists())
		{
			continue;
		}

		if (_infoManager.getBunkers().size() && marine.second == 0)
		{
			for (auto bunker : _infoManager.getBunkers())
			{
				if (bunker->getLoadedUnits().size() < 4 && !bunker->isConstructing())
				{
					marine.first->load(bunker);
					loading = true;
					break;
				}
				else if (bunker->isConstructing())
				{
					marine.first->move(bunker->getPosition());
					loading = true;
					break;
				}
			}
		}

		if (marine.first->isLoaded() || loading) continue;

		if (marine.second == 0)
		{
			if (assignSquad(marine.first, false, true))
				marine.second = 1;
		}
	}

	// Loop through our medics and make sure they are assigned to a squad
	for (auto & medic : _infoManager.getMedics())
	{
		if (!medic.first->exists())
		{
			continue;
		}

		if (medic.second == 0)
		{
			if (assignSquad(medic.first, false, true))
				medic.second = 1;
		}
	}

	// Loop through our Vultures and make sure they are assigned to a squad
	for (auto & vulture : _infoManager.getVultures())
	{
		if (!vulture.first->exists())
		{
			continue;
		}

		if (vulture.second == 0)
		{
			if (assignSquad(vulture.first, false, true))
				vulture.second = 1;
		}
	}

	// Loop through our tanks and make sure they are assigned to a squad
	for (auto & tank : _infoManager.getTanks())
	{
		if (!tank.first->exists())
		{
			continue;
		}

		if (tank.second == 0)
		{
			if (assignSquad(tank.first, false, true))
				tank.second = 1;
		}
	}

	// Loop through our Goliaths and make sure they are assigned to a squad
	for (auto & goliath : _infoManager.getGoliaths())
	{
		if (!goliath.first || !goliath.first->exists())
		{
			continue;
		}

		if (goliath.second == 0)
		{
			if (assignSquad(goliath.first, false, true))
			{
				goliath.second = 1;
			}
		}
	}
}

void UnitManager::assignAir(InformationManager & _infoManager)
{
	// Loop through our BCs and make sure they are assigned to a squad
	for (auto & BC : _infoManager.getBCs())
	{
		if (!BC.first || !BC.first->exists() || BC.first->isCompleted())
		{
			continue;
		}

		if (BC.second == 0)
		{
			if (assignAirSquad(BC.first))
			{
				BC.second = 1;
			}
		}
	}
}

/***************************************************************
* Field Engineer functions
***************************************************************/
void UnitManager::assignFieldEngineers(InformationManager & _infoManager)
{
	if (_infoManager.getWorkers().size())
	{
		std::map<BWAPI::Unit, BWEM::Base *>::iterator & engineer = _infoManager.getWorkers().begin();

		if (engineer->first && engineer->first->exists())
		{
			if (engineer->first->getType() != BWAPI::UnitTypes::Terran_SCV)
			{
				BWAPI::Broodwar << engineer->first->getType() << " found in worker list." << std::endl;
				return;
			}

			_infoManager.getFieldEngineers().push_back(engineer->first);

			for (auto & base : _infoManager.getOwnedBases())
			{
				if (base.second == engineer->second)
				{
					for (auto & assignment : base.second->getMineralAssignments())
					{
						for (auto & mineralWorker : assignment.second)
						{
							if (mineralWorker == engineer->first)
							{
								assignment.second.erase(mineralWorker);
								break;
							}
						}
					}

					for (auto & assignment : base.second->getRefineryAssignments())
					{
						for (auto & gasWorker : assignment.second)
						{
							if (gasWorker == engineer->first)
							{
								assignment.second.erase(gasWorker);
								break;
							}
						}
					}

					break;
				}
			}

			_infoManager.getWorkers().erase(engineer);
		}
	}
}

void UnitManager::checkDeadEngineers(InformationManager & _infoManager)
{
	if (!_infoManager.getFieldEngineers().size())
		return;

	std::list<BWAPI::Unit> & _fieldEngineers = _infoManager.getFieldEngineers();
	for (std::list<BWAPI::Unit>::iterator deadEngineer = _fieldEngineers.begin(); deadEngineer != _fieldEngineers.end();)
	{
		if (!(*deadEngineer) || !(*deadEngineer)->exists())
		{
			deadEngineer = _fieldEngineers.erase(deadEngineer);
		}
		else
		{
			deadEngineer++;
		}
	}
}

void UnitManager::handleFieldEngineers(InformationManager & _infoManager, BWAPI::TilePosition nextUp)
{
	std::list<Squad> _squads;
	std::list<BWAPI::Unit> _injuredTanks;
	_injuredTanks.clear();
	BWAPI::Unit closestTankToTarget = NULL;
	if (_infoManager.isMech(_infoManager.getStrategy()))
		_squads = _mechSquads;
	else
		_squads = _allInSquad;

	for (auto squad : _squads)
	{
		if (squad.getTanks().size())
		{
			for (auto tank : squad.getTanks())
			{
				if (!tank.first || !tank.first->exists())
					continue;

				if (tank.first->getHitPoints() < tank.first->getType().maxHitPoints())
				{
					_injuredTanks.push_back(tank.first);
				}

				if (closestTankToTarget == NULL)
				{
					closestTankToTarget = tank.first;
					continue;
				}

				if (_infoManager.getEnemyBases().size() && 
					tank.first->getDistance(_infoManager.getEnemyBases().begin()->first) < closestTankToTarget->getDistance(_infoManager.getEnemyBases().begin()->first))
				{
					closestTankToTarget = tank.first;
				}
				else if (!_infoManager.getEnemyBases().size() && _infoManager.getEnemyBuildingPositions().size() &&
					tank.first->getDistance(_infoManager.getEnemyBuildingPositions().front()) < closestTankToTarget->getDistance(_infoManager.getEnemyBuildingPositions().front()))
				{
					closestTankToTarget = tank.first;
				}
				else if (!_infoManager.getEnemyBases().size() && _infoManager.getEnemyBuildingPositions().size() &&
					tank.first->getDistance(BWAPI::Position(nextUp)) < closestTankToTarget->getDistance(BWAPI::Position(nextUp)))
				{
					closestTankToTarget = tank.first;
				}
			}
		}
	}

	if (_injuredTanks.size())
	{
		BWAPI::Unit closestInjuredTank = _injuredTanks.front();

		for (auto tank : _injuredTanks)
		{
			if (_infoManager.getEnemyBases().size() &&
				tank->getDistance(_infoManager.getEnemyBases().begin()->first) < closestTankToTarget->getDistance(_infoManager.getEnemyBases().begin()->first))
			{
				closestInjuredTank = tank;
			}
			else if (!_infoManager.getEnemyBases().size() && _infoManager.getEnemyBuildingPositions().size() &&
				tank->getDistance(_infoManager.getEnemyBuildingPositions().front()) < closestTankToTarget->getDistance(_infoManager.getEnemyBuildingPositions().front()))
			{
				closestInjuredTank = tank;
			}
			else if (!_infoManager.getEnemyBases().size() && _infoManager.getEnemyBuildingPositions().size() &&
				tank->getDistance(BWAPI::Position(nextUp)) < closestTankToTarget->getDistance(BWAPI::Position(nextUp)))
			{
				closestInjuredTank = tank;
			}
		}

		for (auto & engineer : _infoManager.getFieldEngineers())
		{
			if (!engineer || !engineer->exists())
				continue;
			else if (!engineer->isRepairing())
				engineer->repair(closestInjuredTank);
		}
	}
	else
	{
		for (auto & engineer : _infoManager.getFieldEngineers())
		{
			if (!engineer || !engineer->exists())
				continue;
			else if (closestTankToTarget == NULL || !closestTankToTarget->exists())
				engineer->move(_infoManager.getOwnedBases().begin()->first);
			else if (!_infoManager.closeEnough(engineer->getPosition(), closestTankToTarget->getPosition()))
				engineer->move(closestTankToTarget->getPosition());
		}
	}
}

/***************************************************************
* Where are we sending our building scouts for our mech build?
***************************************************************/
void insanitybot::UnitManager::handleFloaters(InformationManager & _infoManager, BWAPI::TilePosition nextUp)
{

	BWAPI::Position closest = _infoManager.getNaturalChokePos();
	std::list<Squad> _squads;
	_squads = _mechSquads;

	// No need to sacrifice perfectly good structures to cannon rushes
	if (_infoManager.getOwnedBases().size() < 2)
	{
		if (_infoManager.getBunkers().size())
		{
			closest = _infoManager.getBunkers().front()->getPosition();
		}
		else if (_infoManager.getCommandCenters().size())
		{
			closest = _infoManager.getCommandCenters().front()->getPosition();
		}
	}

	// Loop through our floating buildings and move them to scouting locations for our tanks
	for (auto & floater : _infoManager.getFloatingBuildings())
	{
		if (!floater || !floater->exists())
			continue;

		int distance = 99999;

		if (_infoManager.getEnemyBases().size())
		{
			for (auto squad : _squads)
			{
				if (sqrt(((squad.getSquadPosition().x - _infoManager.getEnemyBases().begin()->first.x) * (squad.getSquadPosition().x - _infoManager.getEnemyBases().begin()->first.x)) +
					((squad.getSquadPosition().y - _infoManager.getEnemyBases().begin()->first.y)) * (squad.getSquadPosition().y - _infoManager.getEnemyBases().begin()->first.y)) < distance)
				{
					distance = sqrt(((squad.getSquadPosition().x - _infoManager.getEnemyBases().begin()->first.x) * (squad.getSquadPosition().x - _infoManager.getEnemyBases().begin()->first.x)) +
						((squad.getSquadPosition().y - _infoManager.getEnemyBases().begin()->first.y)) * (squad.getSquadPosition().y - _infoManager.getEnemyBases().begin()->first.y));
					closest = squad.getSquadPosition();
				}
			}

			if (floater->getDistance(closest) < 64)
			{
				floater->move(_infoManager.getEnemyBases().begin()->first);
			}
			else if (floater->getDistance(closest) > 164)
			{
				floater->move(closest);
			}

		}
		else if (_infoManager.getEnemyBuildingPositions().size())
		{
			for (auto squad : _squads)
			{
				if (sqrt(((squad.getSquadPosition().x - _infoManager.getEnemyBuildingPositions().front().x) * (squad.getSquadPosition().x - _infoManager.getEnemyBuildingPositions().front().x)) +
					((squad.getSquadPosition().y - _infoManager.getEnemyBuildingPositions().front().y)) * (squad.getSquadPosition().y - _infoManager.getEnemyBuildingPositions().front().y)) < distance)
				{
					distance = sqrt(((squad.getSquadPosition().x - _infoManager.getEnemyBuildingPositions().front().x) * (squad.getSquadPosition().x - _infoManager.getEnemyBuildingPositions().front().x)) +
						((squad.getSquadPosition().y - _infoManager.getEnemyBuildingPositions().front().y)) * (squad.getSquadPosition().y - _infoManager.getEnemyBuildingPositions().front().y));
					closest = squad.getSquadPosition();
				}
			}

			if (floater->getDistance(closest) < 64)
			{
				floater->move(_infoManager.getEnemyBuildingPositions().front());
			}
			else if (floater->getDistance(closest) > 128)
			{
				floater->move(closest);
			}
		}
		else
		{
			for (auto squad : _squads)
			{
				if (sqrt(((squad.getSquadPosition().x - BWAPI::Position(nextUp).x) * (squad.getSquadPosition().x - BWAPI::Position(nextUp).x)) +
					((squad.getSquadPosition().y - BWAPI::Position(nextUp).y)) * (squad.getSquadPosition().y - BWAPI::Position(nextUp).y)) < distance)
				{
					distance = sqrt(((squad.getSquadPosition().x - BWAPI::Position(nextUp).x) * (squad.getSquadPosition().x - BWAPI::Position(nextUp).x)) +
						((squad.getSquadPosition().y - BWAPI::Position(nextUp).y)) * (squad.getSquadPosition().y - BWAPI::Position(nextUp).y));
					closest = squad.getSquadPosition();
				}
			}

			if (floater->getDistance(closest) < 64)
			{
				floater->move(BWAPI::Position(BWAPI::Position(nextUp).x, BWAPI::Position(nextUp).y));
			}
			else if (floater->getDistance(closest) > 128)
			{
				floater->move(closest);
			}
		}
	}
}

/***************************************************************
* Drop off island worker if we have one, drop enemy bases if not
****************************************************************/
void insanitybot::UnitManager::handleDropships(InformationManager & _infoManager)
{
	for (auto & dropship : _infoManager.getDropships())
	{
		if (!dropship.first || !dropship.first->exists())
		{
			continue;
		}

		if (_infoManager.getIslandBuilder() && 
			(!dropping || (dropping && !dropship.first->getLoadedUnits().size())))
		{
			dropping = false;
			BWAPI::Unit & islandBuilder = _infoManager.getIslandBuilder();
			if (!islandBuilder->isLoaded())
			{
				bool inPosition = false;

				for (auto base : _infoManager.getIslandBases())
				{
					if (abs(dropship.first->getPosition().x - base.first.x) <= 300 && abs(dropship.first->getPosition().y - base.first.y) <= 300)
					{
						inPosition = true;
					}
				}

				if (!inPosition)
				{
					dropship.first->load(islandBuilder);
				}
			}
			else
			{
				int shortest = 9999999;
				BWAPI::Position destination;
				for (auto base : _infoManager.getIslandBases())
				{
					if (dropship.first->getDistance(base.first) < shortest)
					{
						shortest = dropship.first->getDistance(base.first);
						destination = base.first;
					}
				}

				if (abs(dropship.first->getPosition().x - destination.x) <= 128 && abs(dropship.first->getPosition().y - destination.y) <= 128)
				{
					dropship.first->unloadAll();
				}
				else
				{
					dropship.first->move(destination);
				}
			}
		}
		else if (!_dropSquad.size() && !dropping && !loadingDrop &&
			BWAPI::Broodwar->self()->deadUnitCount(BWAPI::UnitTypes::Terran_Dropship) <= 4)
		{
			_dropSquad.push_back(Squad(dropship.first, false));
			loadingDrop = true;
		}
		else if (_dropSquad.size() && loadingDrop)
		{
			if (dropship.first->getSpaceRemaining())
				_dropSquad.front().loadDrop(dropship.first);
			else if (!dropship.first->getSpaceRemaining() &&
					dropship.first->getHitPoints() == dropship.first->getType().maxHitPoints())
			{
				loadingDrop = false;
				dropping = true;
			}
		}
		else if (dropping && dropship.first->getLoadedUnits().size() > 0)
		{
			const BWAPI::Position target = _infoManager.getDropLocation(dropship.first);

			if (_dropSquad.size() && _dropSquad.front().getSquadDropTarget() == BWAPI::Position(0, 0))
				_dropSquad.front().setSquadDropTarget(target);

			if ((dropship.first->getHitPoints() < 50 || target && dropship.first->getDistance(target) < 300) &&
				dropship.first->canUnloadAtPosition(dropship.first->getPosition()))
			{
				// Make sure the squad knows where it is going after boots hit the ground
				if (_dropSquad.size())
					_dropSquad.front().setSquadDropTarget(target);

				if (dropship.first->isIdle())
				{
					dropship.first->move(target);
					return;
				}

				// get the unit's current command
				BWAPI::UnitCommand currentCommand(dropship.first->getLastCommand());

				// If we've already ordered unloading, wait.
				if ((currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All || currentCommand.getType() == BWAPI::UnitCommandTypes::Unload_All_Position) &&
					BWAPI::Broodwar->getFrameCount() - dropship.first->getLastCommandFrame() < 300)
				{
					return;
				}

				dropship.first->unloadAll(dropship.first->getPosition());
			}
			else
				followPerimiter(dropship.first, target);
		}
		else if (dropping && !dropship.first->getLoadedUnits().size())
		{
			_waypoints.clear();
			_direction = 0;

			if (!_infoManager.closeEnough(dropship.first->getPosition(), BWAPI::Position(_infoManager.getMainPosition())))
				dropship.first->move(BWAPI::Position(_infoManager.getMainPosition()));

			if (!_dropSquad.front().dropSquadSize())
			{
				dropping = false;
				loadingDrop = true;
			}
		}

	}
}

BWAPI::Position insanitybot::UnitManager::TileCenter(const BWAPI::TilePosition & tile)
{
	return BWAPI::Position(tile) + BWAPI::Position(16, 16);
}

// Turn an integer (possibly negative) into a valid waypoint index.
// The waypoints form a loop. so moving to the next or previous one is always possible.
// This calculation is also used in finding the shortest path around the map. Then
// i may be as small as -_waypoints.size() + 1.
int insanitybot::UnitManager::waypointIndex(int i)
{
	if (!_waypoints.size())
	{
		BWAPI::Broodwar << "Waypoint Index, no waypoints" << std::endl;
		return -1;
	}

	const int m = int(_waypoints.size());
	return ((i % m) + m) % m;
}

// The index can be any integer. It gets mapped to a correct index first.
const BWAPI::Position & insanitybot::UnitManager::waypoint(int i)
{
	return _waypoints[waypointIndex(i)];
}

// Waypoints around the edge of the map
void insanitybot::UnitManager::calculateWaypoints()
{
	// Tile coordinates.
	int minX = 1;
	int minY = 1;
	int maxX = BWAPI::Broodwar->mapWidth() - 1;
	int maxY = BWAPI::Broodwar->mapHeight() - 1;

	// Add vertices down the left edge.
	for (int y = minY; y <= maxY; y += WaypointSpacing)
	{
		_waypoints.push_back(TileCenter(BWAPI::TilePosition(minX, y)));
	}
	// Add vertices across the bottom.
	for (int x = minX; x <= maxX; x += WaypointSpacing)
	{
		_waypoints.push_back(TileCenter(BWAPI::TilePosition(x, maxY)));
	}
	// Add vertices up the right edge.
	for (int y = maxY; y >= minY; y -= WaypointSpacing)
	{
		_waypoints.push_back(TileCenter(BWAPI::TilePosition(maxX, y)));
	}
	// Add vertices across the top back to the origin.
	for (int x = maxX; x >= minX; x -= WaypointSpacing)
	{
		_waypoints.push_back(TileCenter(BWAPI::TilePosition(x, minY)));
	}
}

// Decide which direction to go, then follow the perimeter to the destination.
// Called only when the transport exists and is loaded.
void insanitybot::UnitManager::followPerimiter(BWAPI::Unit airship, BWAPI::Position target)
{
	if (!airship || !airship->exists())
	{
		return;
	}

	// Place a loop of points around the edge of the map, to use as waypoints.
	if (_waypoints.empty())
	{
		calculateWaypoints();
	}

	// To follow the waypoints around the edge of the map, we need these things:
	// The initial waypoint index, the final waypoint index near the target,
	// the direction to follow (+1 or -1), and the _target.
	// direction == 0 means we haven't decided which direction to go around,
	// and none of them is set yet.
	if (_direction == 0)
	{
		// Find the start and end waypoints by brute force.
		int startDistance = 999999;
		double endDistance = 999999.9;
		for (size_t i = 0; i < _waypoints.size(); ++i)
		{
			const BWAPI::Position & waypoint = _waypoints[i];
			if (airship->getDistance(waypoint) < startDistance)
			{
				startDistance = airship->getDistance(waypoint);
				_nextWaypointIndex = i;
			}
			if (target.getDistance(waypoint) < endDistance)
			{
				endDistance = target.getDistance(waypoint);
				_lastWaypointIndex = i;
			}
		}

		// Decide which direction around the map is shorter.
		int counterclockwise = waypointIndex(_lastWaypointIndex - _nextWaypointIndex);
		int clockwise = waypointIndex(_nextWaypointIndex - _lastWaypointIndex);
		_direction = (counterclockwise <= clockwise) ? 1 : -1;
	}

	// Everything is set. Do the movement.

	// If we're near the destination, go straight there.
	if (airship->getDistance(waypoint(_lastWaypointIndex)) < 2 * 32 * WaypointSpacing)
	{
		airship->move(target);
	}
	else
	{
		// If the second waypoint ahead is close enough (1.5 waypoint distances), make it the next waypoint.
		if (airship->getDistance(waypoint(_nextWaypointIndex + _direction)) < 48 * WaypointSpacing)
		{
			_nextWaypointIndex = waypointIndex(_nextWaypointIndex + _direction);
		}

		// Aim for the second waypoint ahead.
		const BWAPI::Position & destination = waypoint(_nextWaypointIndex + _direction);

		/*if (Config::Debug::DrawUnitTargets)
		{
			BWAPI::Broodwar->drawCircleMap(destination, 5, BWAPI::Colors::Yellow, true);
		}*/

		airship->move(destination);
	}
}

/***************************************************************
* Will return true if we found a valid target for our science 
* vessel to irradiate
****************************************************************/
bool insanitybot::UnitManager::irradiateTarget(BWAPI::Unit vessel)
{
	if (vessel->getEnergy() < 75 || !BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Irradiate))
		return false;

	BWAPI::Unit target = NULL;
	bool anyTarget = vessel->getEnergy() > 150;
	int closestDistance = 999999;

	for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (!enemy)
			continue;

		// Make sure it is a valid target
		if (enemy->getType().isOrganic() && !enemy->getType().isBuilding() && enemy->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			enemy->getType() != BWAPI::UnitTypes::Zerg_Cocoon && enemy->getType() != BWAPI::UnitTypes::Zerg_Broodling &&
			enemy->getType() != BWAPI::UnitTypes::Zerg_Zergling && enemy->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			enemy->getType() != BWAPI::UnitTypes::Zerg_Lurker_Egg &&
			!enemy->isIrradiated() && !enemy->isInvincible() && notInIrradiateDB(enemy))
		{
			// Hoping to prioritize high value targets
			if ((enemy->getType() == BWAPI::UnitTypes::Zerg_Defiler ||
				enemy->getType() == BWAPI::UnitTypes::Terran_Ghost ||
				enemy->getType() == BWAPI::UnitTypes::Protoss_High_Templar) && vessel->getDistance(enemy) - 200 < closestDistance)
			{
				target = enemy;
				closestDistance = vessel->getDistance(enemy) - 200;
			}
			else if ((enemy->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
				enemy->getType() == BWAPI::UnitTypes::Zerg_Mutalisk ||
				enemy->getType() == BWAPI::UnitTypes::Zerg_Ultralisk ||
				enemy->getType() == BWAPI::UnitTypes::Zerg_Queen ||
				enemy->getType() == BWAPI::UnitTypes::Zerg_Guardian ||
				enemy->getType() == BWAPI::UnitTypes::Zerg_Devourer ||
				enemy->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar) && vessel->getDistance(enemy) - 75 < closestDistance)
			{
				target = enemy;
				closestDistance = vessel->getDistance(enemy) - 75;
			}
			else if (vessel->getDistance(enemy) < closestDistance && anyTarget)
			{
				target = enemy;
				closestDistance = vessel->getDistance(enemy);
			}
		}
	}

	if (target)
	{
		vessel->useTech(BWAPI::TechTypes::Irradiate, target);
		_irradiateDB.insert(std::pair<BWAPI::Unit, std::pair<BWAPI::Unit, int>>(vessel, std::pair<BWAPI::Unit, int>(target, BWAPI::Broodwar->getFrameCount())));
	}
	return target;
}

// Simple check if we've potentially already marked the target to be irradiated
bool insanitybot::UnitManager::notInIrradiateDB(BWAPI::Unit potentialTarget)
{
	for (auto target : _irradiateDB)
	{
		if (target.second.first == potentialTarget)
		{
			return false;
		}
	}
	return true;
}

/*****************************************************************************
* Similar code to irradiation but for d-matrix primarily on tanks
******************************************************************************/
bool insanitybot::UnitManager::matrixTarget(BWAPI::Unit vessel)
{
	if (vessel->getEnergy() < 100)
		return false;

	BWAPI::Unit target = NULL;
	bool anyTarget = vessel->getEnergy() > 140;
	int closestDistance = 999999;

	for (auto friendly : BWAPI::Broodwar->self()->getUnits())
	{

		if (!friendly || !friendly->exists())
			continue;

		if (vessel->getDistance(friendly) > 600 ||
			friendly->getType().isBuilding() || 
			!friendly->isUnderAttack() ||
			friendly->isIrradiated() || friendly->isInvincible() ||
			friendly->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
			friendly->getType() == BWAPI::UnitTypes::Terran_SCV ||
			friendly->getType() == BWAPI::UnitTypes::Terran_Medic ||
			friendly->getType() == BWAPI::UnitTypes::Terran_Science_Vessel)
			continue;

		// Hoping to prioritize high value targets
		if ((friendly->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
			friendly->getType() == BWAPI::UnitTypes::Terran_Ghost) && 
			vessel->getDistance(friendly) - 100 < closestDistance)
		{
			target = friendly;
			closestDistance = vessel->getDistance(friendly) - 100;
		}
		else if (vessel->getDistance(friendly) < closestDistance && anyTarget)
		{
			target = friendly;
			closestDistance = vessel->getDistance(friendly);
		}
	}

	if (target)
	{
		vessel->useTech(BWAPI::TechTypes::Defensive_Matrix, target);
		_MatrixDB.insert(std::pair<BWAPI::Unit, std::pair<BWAPI::Unit, int>>(vessel, std::pair<BWAPI::Unit, int>(target, BWAPI::Broodwar->getFrameCount())));
	}
	return target;
}

// Simple check if we've potentially already marked the target to be irradiated
bool insanitybot::UnitManager::notInMatrixDB(BWAPI::Unit potentialTarget)
{
	for (auto target : _MatrixDB)
	{
		if (target.second.first == potentialTarget)
		{
			return false;
		}
	}
	return true;
}

/*****************************************************************************
* Check the flare database and erase any targets that have taken too long
* to flare.
******************************************************************************/
void insanitybot::UnitManager::checkFlareDBForTimeout()
{
	for (std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>::iterator target = _flareBD.begin(); target != _flareBD.end();)
	{
		if (BWAPI::Broodwar->getFrameCount() - target->second.second > 72)
		{
			target = _flareBD.erase(target);
			continue;
		}

		target++;
	}
}

/***************************************************************
* Display squad information on the map
****************************************************************/
void insanitybot::UnitManager::infoText()
{

	//BWAPI::Broodwar->drawTextScreen(50, 90, "numFrontierSquads: %d", _frontierSquads.size());
	//BWAPI::Broodwar->drawTextScreen(50, 100, "frontierSquadSizeLimit: %d", frontierSquadSizeLimit);

	BWAPI::Broodwar->drawTextScreen(200, 100, "numInfSquads: %d", _infantrySquads.size());
	BWAPI::Broodwar->drawTextScreen(200, 110, "numMecSquads: %d", _mechSquads.size());

	std::vector<std::string> infantrySquadCallSigns{ "Overlord", "Hunter 1-1", "Hunter 1-2", "Hunter 1-3", "Hunter 1-4", "Hunter 1-5", "Hunter 1-6", "Hunter 1-7", "Hunter 1-8", "Hunter 1-9", "Hunter 1-10", "Hunter 1 - 11", "Hunter 1-12" 
	, "Hunter 1-13" , "Hunter 1-14" , "Hunter 1-15", "Hunter 1-16" };
	std::vector<std::string> mechSquadCallSigns{ "Overlord", "Broadsword 1-1", "Broadsword 1-2", "Broadsword 1-3", "Broadsword 1-4", "Broadsword 1-5", "Broadsword 1-6", "Broadsword 1-7", "Broadsword 1-8", "Broadsword 1-9", "Broadsword 1-10"
	, "Broadsword 1-11", "Broadsword 1-12", "Broadsword 1-13", "Broadsword 1-14", "Broadsword 1-15", "Broadsword 1-16", "Broadsword 1-17", "Broadsword 1-18", "Broadsword 1-19"};
	std::vector<std::string> defensiveCallsings{ "Overlord", "Aegis 1-1", "Aegis 1-2", "Aegis 1-3", "Aegis 1-4", "Aegis 1-5", "Aegis 1-6", "Aegis 1-7", "Aegis 1-8", "Aegis 1-9" };
	std::vector<std::string> dropSquadCallSigns{ "Overlord", "Bandit 1-1", "Bandit 1-2", "Bandit 1-3", "Bandit 1-4", "Bandit 1-5" };
	std::vector<std::string> specialistSquadCallSigns{ "Overlord", "Delta 1-1", "Delta 1-2", "Delta 1-3", "Delta 1-4", "Delta 1-5" };

	int count = 1;
	for (auto squad : _infantrySquads)
	{
		BWAPI::Broodwar->drawTextMap(squad.getSquadPosition(), "%s", infantrySquadCallSigns[count].c_str());
		count++;
	}

	count = 1;
	for (auto squad : _mechSquads)
	{
		BWAPI::Broodwar->drawTextMap(squad.getSquadPosition(), "%s", mechSquadCallSigns[count].c_str());
		count++;
	}

	count = 1;
	for (auto squad : _defensiveSquads)
	{
		BWAPI::Broodwar->drawTextMap(squad.getSquadPosition(), "%s", defensiveCallsings[count].c_str());
		count++;
	}

	count = 1;
	for (auto squad : _dropSquad)
	{
		BWAPI::Broodwar->drawTextMap(squad.getSquadPosition(), "%s", dropSquadCallSigns[count].c_str());
		count++;
	}

	count = 1;
	for (auto squad : _specialistSquad)
	{
		BWAPI::Broodwar->drawTextMap(squad.getSquadPosition(), "%s", specialistSquadCallSigns[count].c_str());
		count++;
	}

	/*for (auto enemy : _flareBD)
	{
		if (enemy.second.first && enemy.second.first->exists())
		{
			BWAPI::Broodwar->drawTextMap(enemy.second.first->getPosition(), "inDB");
		}
	}

	for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (enemy && enemy->exists())
		{
			if (enemy->isBlind())
			{
				BWAPI::Broodwar->drawCircleMap(enemy->getPosition(), 5, BWAPI::Colors::Grey, true);
			}
		}
	}*/
}
