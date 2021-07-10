#include "UnitManager.h"
#include <math.h>

namespace { auto & theMap = BWEM::Map::Instance(); }

using namespace insanitybot;

insanitybot::UnitManager::UnitManager()
{
	infantrySquadSizeLimit = 15;
	mechSquadSizeLimit = 9;
	needDefense = false;
}

void UnitManager::update(InformationManager & _infoManager)
{
	if (_infoManager.getScout())
	{
		handleScout(_infoManager.getScout());
	}

	// If we don't know where the enemy is, scout around
	std::vector<BWAPI::TilePosition>& squadScoutLocation = _infoManager.getSquadScoutLocations();
	BWAPI::TilePosition nextUp = squadScoutLocation.front();
	if (BWAPI::Broodwar->isVisible(nextUp.x, nextUp.y))
	{
		squadScoutLocation.push_back(squadScoutLocation.front());
		squadScoutLocation.erase(squadScoutLocation.begin());
		nextUp = squadScoutLocation.front();
	}

	if (_infoManager.getStrategy() == "SKTerran")
	{	// Loop through our marines and make sure they are assigned to a squad
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
				if (assignSquad(marine.first, true))
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
				if (assignSquad(medic.first, true))
					medic.second = 1;
			}
		}
	}
	else if (_infoManager.getStrategy() == "Mech")
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
				if (assignSquad(vulture.first, false))
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
				if (assignSquad(tank.first, false))
					tank.second = 1;
			}
		}

		// Loop through our Goliaths and make sure they are assigned to a squad
		for (auto & goliath : _infoManager.getGoliaths())
		{
			if (!goliath.first || !goliath.first->exists())
			{
				BWAPI::Broodwar << "Goliath in list doesn't exist" << std::endl;
				continue;
			}

			if (goliath.second == 0)
			{
				BWAPI::Broodwar << "Goliath unassigned found" << std::endl;
				if (assignSquad(goliath.first, false))
				{
					goliath.second = 1;
					BWAPI::Broodwar << "Goliath assigned to squad" << std::endl;
				}
			}
		}

		// Loop through our floating buildings and move them to scouting locations for our tanks
		for (auto & floater : _infoManager.getFloatingBuildings())
		{
			if (_infoManager.getEnemyBases().size())
			{
				BWAPI::Position closest = _infoManager.getNaturalChokePos();
				int distance = 99999;
				std::list<Squad> _squads;
				if (_infoManager.getStrategy() == "SKTerran")
				{
					_squads = _infantrySquads;
				}
				else if (_infoManager.getStrategy() == "Mech")
				{
					_squads = _mechSquads;
				}

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
				else if (floater->getDistance(closest) > 128)
				{
					floater->move(closest);
				}
				
			}
			else if (_infoManager.getEnemyBuildingPositions().size())
			{
				BWAPI::Position closest = _infoManager.getNaturalChokePos();
				int distance = 99999;
				std::list<Squad> _squads;
				if (_infoManager.getStrategy() == "SKTerran")
				{
					_squads = _infantrySquads;
				}
				else if (_infoManager.getStrategy() == "Mech")
				{
					_squads = _mechSquads;
				}

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
				BWAPI::Position closest = _infoManager.getNaturalChokePos();
				int distance = 99999;
				std::list<Squad> _squads;
				if (_infoManager.getStrategy() == "SKTerran")
				{
					_squads = _infantrySquads;
				}
				else if (_infoManager.getStrategy() == "Mech")
				{
					_squads = _mechSquads;
				}

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

	// Check if we need a defensive squad
	if (_infoManager.shouldHaveDefenseSquad(false) && !_defensiveSquads.size())
	{
		if (_infantrySquads.size())
		{
			for (std::list<Squad>::iterator & selection = _infantrySquads.begin(); selection != _infantrySquads.end(); selection++)
			{
				if (!selection->isGoodToAttack())
				{
					BWAPI::Broodwar << "Defense Squad Created" << std::endl;
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
						BWAPI::Broodwar << "Defense Squad Created" << std::endl;
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
			if (!unit)
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
					if (_infoManager.getStrategy() == "SKTerran")
					{
						squad->setHaveGathered(false);
						_infantrySquads.push_back(*squad);
						_defensiveSquads.erase(squad);
						continue;
					}
					else if (_infoManager.getStrategy() == "Mech")
					{
						squad->setHaveGathered(false);
						_mechSquads.push_back(*squad);
						_defensiveSquads.erase(squad);
						continue;
					}
				}
			}
			else if (target->getPlayer() == BWAPI::Broodwar->enemy())
			{
				if (_infoManager.getStrategy() == "SKTerran")
				{
					// Send mostly full squads to engage
					if (squad->infantrySquadSize() >= 10)
					{
						squad->setDefenseLastNeededFrame(BWAPI::Broodwar->getFrameCount());
						squad->attack(target->getPosition(), BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, BWAPI::Position(_infoManager.getMainPosition()).y));
					}
					// Else, fall back to a defensive point and hope reinforcements pop soon
					else
					{
						squad->setDefenseLastNeededFrame(BWAPI::Broodwar->getFrameCount());
						BWAPI::Position defensePoint = BWAPI::Position((BWAPI::Position(_infoManager.getMainPosition()).x + target->getPosition().x) / 2, (BWAPI::Position(_infoManager.getMainPosition()).y + target->getPosition().y) / 2);
						squad->attack(defensePoint, BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, BWAPI::Position(_infoManager.getMainPosition()).y));
					}
				}
				else if (_infoManager.getStrategy() == "Mech")
				{
					// Don't send squads with no anti air to defend air attacks
					if (_infoManager.enemyHasAir() && squad->numGoliaths() < 1)
					{
						_defensiveSquads.erase(squad);
						break;
					}
					// Send mostly full squads to engage
					else if (squad->mechSquadSize() >= 7)
					{
						squad->setDefenseLastNeededFrame(BWAPI::Broodwar->getFrameCount());
						squad->attack(target->getPosition(), BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, BWAPI::Position(_infoManager.getMainPosition()).y));
					}
					// Else, fall back to a defensive point and hope reinforcements pop soon
					else
					{
						squad->setDefenseLastNeededFrame(BWAPI::Broodwar->getFrameCount());
						BWAPI::Position defensePoint = BWAPI::Position((BWAPI::Position(_infoManager.getMainPosition()).x + target->getPosition().x) / 2, (BWAPI::Position(_infoManager.getMainPosition()).y + target->getPosition().y) / 2);
						squad->attack(defensePoint, BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, BWAPI::Position(_infoManager.getMainPosition()).y));
					}
				}
			}
			else if (target->getPlayer() == BWAPI::Broodwar->neutral())
			{
				squad->setDefenseLastNeededFrame(BWAPI::Broodwar->getFrameCount());
				squad->attack(target);
			}
		}
	}

	// If we have squads, loop through to make sure they are where they need to be
	// Bio squads
	if (_infantrySquads.size())
	{
		for (auto & squad : _infantrySquads)
		{
			if (_infoManager.getAggression() && squad.infantrySquadSize() == infantrySquadSizeLimit)
			{
				squad.setGoodToAttack(true);
			}

			if (squad.isGoodToAttack())
			{
				BWAPI::TilePosition center;
				if (BWAPI::Broodwar->mapHash() == "c8386b87051f6773f6b2681b0e8318244aa086a6") // Neo Moon Glaive, center is unaccessible
					center = BWAPI::TilePosition(BWAPI::TilePosition((BWAPI::Broodwar->mapWidth() + 25) / 2, BWAPI::Broodwar->mapHeight() / 2));
				else if (BWAPI::Broodwar->mapHash() == "9bfc271360fa5bab3707a29e1326b84d0ff58911") // Tao Cross
					center = BWAPI::TilePosition(BWAPI::TilePosition(BWAPI::Broodwar->mapWidth() / 2, (BWAPI::Broodwar->mapHeight() - 20) / 2));
				else
					center = BWAPI::TilePosition(BWAPI::TilePosition(BWAPI::Broodwar->mapWidth() / 2, BWAPI::Broodwar->mapHeight() / 2));

				if (_infoManager.getEnemyBases().size())
				{
					BWAPI::Position forwardPosition;

					if (BWAPI::Broodwar->mapHash() == "e47775e171fe3f67cc2946825f00a6993b5a415e") // La Mancha
					{
						forwardPosition = BWAPI::Position((_infoManager.getEnemyNaturalPos().x * .70) + (BWAPI::Position(center).x * .30), (_infoManager.getEnemyNaturalPos().y * .70) + (BWAPI::Position(center).y * .40));
					}
					else
					{
						forwardPosition = BWAPI::Position((_infoManager.getEnemyNaturalPos().x + BWAPI::Position(center).x) / 2, (_infoManager.getEnemyNaturalPos().y + BWAPI::Position(center).y) / 2);
					}

					squad.attack(_infoManager.getEnemyBases().begin()->first, forwardPosition);
				}
				else if (_infoManager.getEnemyBuildingPositions().size())
				{
					BWAPI::Position forwardPosition = BWAPI::Position(0, 0);
					squad.attack(_infoManager.getEnemyBuildingPositions().front(), forwardPosition);
				}
				else
				{
					BWAPI::Position forwardPosition = BWAPI::Position(0, 0);
					squad.attack(BWAPI::Position(BWAPI::Position(nextUp).x, BWAPI::Position(nextUp).y), forwardPosition);
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
					squad.gather(_infoManager.getBunkers().front()->getPosition());
				else
				{
					BWAPI::Position halfway = BWAPI::Position((BWAPI::Position(_infoManager.getNatPosition()).x + _infoManager.getNaturalChokePos().x) / 2, (BWAPI::Position(_infoManager.getNatPosition()).y + _infoManager.getNaturalChokePos().y) / 2);
					squad.gather(halfway);
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
				BWAPI::TilePosition center;
				if (BWAPI::Broodwar->mapHash() == "c8386b87051f6773f6b2681b0e8318244aa086a6") // Neo Moon Glaive, center is unaccessible
					center = BWAPI::TilePosition(BWAPI::TilePosition((BWAPI::Broodwar->mapWidth() + 25) / 2, BWAPI::Broodwar->mapHeight() / 2));
				else if (BWAPI::Broodwar->mapHash() == "9bfc271360fa5bab3707a29e1326b84d0ff58911") // Tao Cross
					center = BWAPI::TilePosition(BWAPI::TilePosition(BWAPI::Broodwar->mapWidth() / 2, (BWAPI::Broodwar->mapHeight() - 20) / 2));
				else
					center = BWAPI::TilePosition(BWAPI::TilePosition(BWAPI::Broodwar->mapWidth() / 2, BWAPI::Broodwar->mapHeight() / 2));

				if (_infoManager.getEnemyBases().size())
				{
					BWAPI::Position forwardPosition;

					if (BWAPI::Broodwar->mapHash() == "e47775e171fe3f67cc2946825f00a6993b5a415e") // La Mancha
					{
						forwardPosition = BWAPI::Position((_infoManager.getEnemyNaturalPos().x * .70) + (BWAPI::Position(center).x * .30), (_infoManager.getEnemyNaturalPos().y * .70) + (BWAPI::Position(center).y * .40));
					}
					else
					{
						forwardPosition = BWAPI::Position((_infoManager.getEnemyNaturalPos().x + BWAPI::Position(center).x) / 2, (_infoManager.getEnemyNaturalPos().y + BWAPI::Position(center).y) / 2);
					}

					squad.attack(_infoManager.getEnemyBases().begin()->first, forwardPosition);
				}
				else if (_infoManager.getEnemyBuildingPositions().size())
				{
					BWAPI::Position forwardPosition = BWAPI::Position(0, 0);
					squad.attack(_infoManager.getEnemyBuildingPositions().front(), forwardPosition);
				}
				else
				{
					BWAPI::Position forwardPosition = BWAPI::Position(0, 0);
					squad.attack(BWAPI::Position(BWAPI::Position(nextUp).x, BWAPI::Position(nextUp).y), forwardPosition);
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
					squad.gather(_infoManager.getBunkers().front()->getPosition());
				else
				{
					BWAPI::Position halfway = BWAPI::Position((BWAPI::Position(_infoManager.getNatPosition()).x + _infoManager.getNaturalChokePos().x) / 2, (BWAPI::Position(_infoManager.getNatPosition()).y + _infoManager.getNaturalChokePos().y) / 2);
					squad.gather(halfway);
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

	// Dropship
	for (auto & dropship : _infoManager.getDropships())
	{
		if (!dropship.first->exists())
		{
			continue;
		}

		if (_infoManager.getIslandBuilder())
		{
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
	}

	// Vessel handling and spellcasting
	for (auto & vessel : _infoManager.getVessels())
	{
		if (!vessel.first->exists())
		{
			continue;
		}

		for (std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>::iterator target = _irradiateDB.begin(); target != _irradiateDB.end(); target++)
		{
			if (BWAPI::Broodwar->getFrameCount() - target->second.second > 72)
			{
				_irradiateDB.erase(target);
				continue;
			}

			if (target->first == vessel.first)
			{
				if (vessel.first->getEnergy() > 75 && vessel.first->getOrder() != BWAPI::Orders::CastIrradiate)
				{
					vessel.first->useTech(BWAPI::TechTypes::Irradiate, target->second.first);
					goto nextVessel;
				}
			}
		}

		if (!irradiateTarget(vessel.first))
		{
			if (_infoManager.getEnemyBases().size())
			{
				BWAPI::Position closest = _infoManager.getNaturalChokePos();
				int distance = 99999;
				std::list<Squad> _squads;
				if (_infoManager.getStrategy() == "SKTerran")
				{
					_squads = _infantrySquads;
				}
				else if (_infoManager.getStrategy() == "Mech")
				{
					_squads = _mechSquads;
				}
				
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
				BWAPI::Position closest = _infoManager.getNaturalChokePos();
				int distance = 99999;
				std::list<Squad> _squads;
				if (_infoManager.getStrategy() == "SKTerran")
				{
					_squads = _infantrySquads;
				}
				else if (_infoManager.getStrategy() == "Mech")
				{
					_squads = _mechSquads;
				}

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
				BWAPI::Position closest = _infoManager.getNaturalChokePos();
				int distance = 99999;
				std::list<Squad> _squads;
				if (_infoManager.getStrategy() == "SKTerran")
				{
					_squads = _infantrySquads;
				}
				else if (_infoManager.getStrategy() == "Mech")
				{
					_squads = _mechSquads;
				}

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

void UnitManager::handleScout(BWAPI::Unit & _scout)
{
	if (!_scout || !_scout->exists())
		return;

	if (!_scout->isMoving() || _scout->isGatheringMinerals() || _scout->isIdle())
	{
		for (auto location : theMap.StartingLocations())
		{
			if (!BWAPI::Broodwar->isExplored(location))
				_scout->move(BWAPI::Position(location));
		}
	}
}

bool insanitybot::UnitManager::assignSquad(BWAPI::Unit unassigned, bool bio)
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
		else if (_infantrySquads.size())
		{
			for (auto & squad : _infantrySquads)
			{
				if (squad.infantrySquadSize() == infantrySquadSizeLimit || squad.isGoodToAttack())
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

			if (_infantrySquads.size() < 10)
			{
				_infantrySquads.push_back(Squad(unassigned));
				return true;
			}
		}
		else
		{
			_infantrySquads.push_back(Squad(unassigned));
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
		else if (_mechSquads.size())
		{
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
				_mechSquads.push_back(Squad(unassigned));
				return true;
			}
		}
		else
		{
			_mechSquads.push_back(Squad(unassigned));
			return true;
		}
	}

	return false;
}

// Will return a valid target for our science vessel to irradiate
bool insanitybot::UnitManager::irradiateTarget(BWAPI::Unit vessel)
{
	if (vessel->getEnergy() < 75 || !BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Irradiate))
		return false;

	BWAPI::Unit target = NULL;
	bool anyTarget = false;
	int closestDistance = 999999;

	if (vessel->getEnergy() > 150)
		anyTarget = true; 

	for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (!enemy)
			continue;

		// Make sure it is a valid target
		if (enemy->getType().isOrganic() && !enemy->getType().isBuilding() && enemy->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			enemy->getType() != BWAPI::UnitTypes::Zerg_Cocoon && enemy->getType() != BWAPI::UnitTypes::Zerg_Broodling &&
			enemy->getType() != BWAPI::UnitTypes::Zerg_Zergling && enemy->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			enemy->getType() != BWAPI::UnitTypes::Zerg_Lurker_Egg &&
			!enemy->isIrradiated() && !enemy->isInvincible() && notIrradiateTarged(enemy))
		{
			// Hoping to prioritize high value targets
			if ((enemy->getType() == BWAPI::UnitTypes::Zerg_Lurker ||
				enemy->getType() == BWAPI::UnitTypes::Zerg_Mutalisk ||
				enemy->getType() == BWAPI::UnitTypes::Zerg_Ultralisk ||
				enemy->getType() == BWAPI::UnitTypes::Zerg_Queen ||
				enemy->getType() == BWAPI::UnitTypes::Zerg_Guardian ||
				enemy->getType() == BWAPI::UnitTypes::Zerg_Devourer ||
				enemy->getType() == BWAPI::UnitTypes::Protoss_High_Templar ||
				enemy->getType() == BWAPI::UnitTypes::Terran_Ghost) && vessel->getDistance(enemy) - 75 < closestDistance)
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

bool insanitybot::UnitManager::notIrradiateTarged(BWAPI::Unit potentialTarget)
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

void insanitybot::UnitManager::infoText()
{
	BWAPI::Broodwar->drawTextScreen(200, 100, "numInfSquads: %d", _infantrySquads.size());
	BWAPI::Broodwar->drawTextScreen(200, 110, "numMecSquads: %d", _mechSquads.size());

	std::vector<std::string> infantrySquadCallSigns{ "Overlord", "Hunter 1-1", "Hunter 1-2", "Hunter 1-3", "Hunter 1-4", "Hunter 1-5", "Hunter 1-6", "Hunter 1-7", "Hunter 1-8", "Hunter 1-9", "Hunter 1-10" };
	std::vector<std::string> mechSquadCallSigns{ "Overlord", "Broadsword 1-1", "Broadsword 1-2", "Broadsword 1-3", "Broadsword 1-4", "Broadsword 1-5", "Broadsword 1-6", "Broadsword 1-7", "Broadsword 1-8", "Broadsword 1-9", "Broadsword 1-10"
	, "Broadsword 1-11", "Broadsword 1-12", "Broadsword 1-13", "Broadsword 1-14", "Broadsword 1-15", "Broadsword 1-16", "Broadsword 1-17", "Broadsword 1-18", "Broadsword 1-19"};
	std::vector<std::string> defensiveCallsings{ "Overlord", "Aegis 1-1", "Aegis 1-2", "Aegis 1-3", "Aegis 1-4", "Aegis 1-5", "Aegis 1-6", "Aegis 1-7", "Aegis 1-8", "Aegis 1-9" };

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
}
