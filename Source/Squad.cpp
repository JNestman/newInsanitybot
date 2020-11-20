#include "Squad.h"

namespace { auto & theMap = BWEM::Map::Instance(); }

using namespace insanitybot;

insanitybot::Squad::Squad(BWAPI::Unit unit)
{
	goodToAttack = false;
	haveGathered = false;

	_marines.clear();
	_medics.clear();
	_vultures.clear();
	_tanks.clear();
	_goliaths.clear();

	if (unit->getType() == BWAPI::UnitTypes::Terran_Marine)
	{
		_marines.push_back(unit);
	}
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Medic)
	{
		_medics.push_back(unit);
	}
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture)
	{
		_vultures.push_back(unit);
	}
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode || unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
	{
		_tanks.insert(std::pair<BWAPI::Unit, int>(unit, 0));
	}
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Goliath)
	{
		_goliaths.push_back(unit);
	}

	defenseLastNeededFrame = 0;
}

BWAPI::Position insanitybot::Squad::getSquadPosition()
{
	if (infantrySquadSize() + mechSquadSize() == 0)
	{
		return BWAPI::Position(1500, 1500);
	}

	int xsum = 0;
	int ysum = 0;

	if (_marines.size())
	{
		for (auto marine : _marines)
		{
			if (!marine->exists())
				continue;
			xsum += marine->getPosition().x;
			ysum += marine->getPosition().y;
		}
	}

	if (_medics.size())
	{
		for (auto medic : _medics)
		{
			if (!medic->exists())
				continue;
			xsum += medic->getPosition().x;
			ysum += medic->getPosition().y;
		}
	}

	if (_vultures.size())
	{
		for (auto vulture : _vultures)
		{
			if (!vulture->exists())
				continue;
			xsum += vulture->getPosition().x;
			ysum += vulture->getPosition().y;
		}
	}

	if (_tanks.size())
	{
		for (auto tank : _tanks)
		{
			if (!tank.first->exists())
				continue;
			xsum += tank.first->getPosition().x;
			ysum += tank.first->getPosition().y;
		}
	}

	if (_goliaths.size())
	{
		for (auto goliath : _goliaths)
		{
			if (!goliath->exists())
				continue;
			xsum += goliath->getPosition().x;
			ysum += goliath->getPosition().y;
		}
	}

	return BWAPI::Position(xsum / (infantrySquadSize() + mechSquadSize()), ysum / (infantrySquadSize() + mechSquadSize()));
}

bool insanitybot::Squad::removeMarine(BWAPI::Unit unit)
{
	for (std::list<BWAPI::Unit>::iterator marine = _marines.begin(); marine != _marines.end(); marine++)
	{
		if (*marine == unit)
		{
			_marines.erase(marine);

			return true;
		}
	}

	return false;
}

bool insanitybot::Squad::removeMedic(BWAPI::Unit unit)
{
	for (std::list<BWAPI::Unit>::iterator medic = _medics.begin(); medic != _medics.end(); medic++)
	{
		if (*medic == unit)
		{
			_medics.erase(medic);

			return true;
		}
	}

	return false;
}

bool insanitybot::Squad::removeVulture(BWAPI::Unit unit)
{
	for (std::list<BWAPI::Unit>::iterator vulture = _vultures.begin(); vulture != _vultures.end(); vulture++)
	{
		if (*vulture == unit)
		{
			_vultures.erase(vulture);

			return true;
		}
	}

	return false;
}

bool insanitybot::Squad::removeTank(BWAPI::Unit unit)
{
	for (std::map<BWAPI::Unit, int>::iterator tank = _tanks.begin(); tank != _tanks.end(); tank++)
	{
		if (tank->first == unit)
		{
			_tanks.erase(tank);

			return true;
		}
	}

	return false;
}

bool insanitybot::Squad::removeGoliath(BWAPI::Unit unit)
{
	for (std::list<BWAPI::Unit>::iterator goliath = _goliaths.begin(); goliath != _goliaths.end(); goliath++)
	{
		if (*goliath == unit)
		{
			_goliaths.erase(goliath);

			return true;
		}
	}

	return false;
}

void insanitybot::Squad::attack(BWAPI::Position attackPoint, BWAPI::Position forwardGather)
{
	std::list<BWAPI::Unit> injured;
	injured.clear();

	BWAPI::Position approximateSquadPosition = getSquadPosition();

	if (closeEnough(approximateSquadPosition, forwardGather) || forwardGather == BWAPI::Position(0, 0))
		haveGathered = true;

	/****************************************************************************************
	* Bio
	*****************************************************************************************/
	for (auto marine : _marines)
	{
		if (!marine->exists())
		{
			BWAPI::Broodwar << "dead marine in squad list" << std::endl;
			removeMarine(marine);
			continue;
		}

		if (!marine->isAttacking() && !marine->isMoving() && !marine->isUnderAttack())
		{
			if (haveGathered)
			{
				if (!closeEnough(marine->getPosition(), attackPoint))
					marine->attack(attackPoint);
			}
			else
			{
				if (!closeEnough(marine->getPosition(), forwardGather))
				{
					marine->attack(forwardGather);
				}
			}
		}

		if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
		{
			if (marine->isAttacking() && !marine->isStimmed() && 
				(marine->getHitPoints() == marine->getType().maxHitPoints() || _medics.size()))
			{
				marine->useTech(BWAPI::TechTypes::Stim_Packs);
			}
		}

		if (marine->isCompleted() && marine->getHitPoints() < marine->getType().maxHitPoints())
		{
			if (!marine->exists())
				continue;
			injured.push_back(marine);
		}
	}

	int spread = 0;

	for (auto medic : _medics)
	{
		if (injured.size())
		{
			for (auto injuredSquadmate : injured)
			{
				if (!injuredSquadmate->isBeingHealed())
				{
					medic->useTech(BWAPI::TechTypes::Healing, injuredSquadmate);
					break;
				}
			}
		}
		else
		{
			// Effort to prevent medics crowding one marine and blocking movement
			if (_marines.size() > 1)
			{
				std::list<BWAPI::Unit>::iterator buddy = _marines.begin();
				if (spread)
				{
					buddy++;
				}

				spread++;
				medic->move((*buddy)->getPosition());
			}
			else if (_marines.size())
			{
				medic->move(_marines.front()->getPosition());
			}
				
		}
	}

	/****************************************************************************************
	* Mech
	*****************************************************************************************/
	int closest = 999999;
	BWAPI::Unit closestTankToTarget = NULL;
	for (auto tank : _tanks)
	{
		if (!tank.first->exists())
		{
			BWAPI::Broodwar << "dead tank in squad list" << std::endl;
			removeTank(tank.first);
			continue;
		}

		if (!haveGathered && tank.first->getDistance(forwardGather) < closest)
		{
			closestTankToTarget = tank.first;
			closest = tank.first->getDistance(forwardGather);
		}
		else if (haveGathered && tank.first->getDistance(attackPoint) < closest)
		{
			closestTankToTarget = tank.first;

			closest = tank.first->getDistance(attackPoint);
		}
	}

	for (auto & tank : _tanks)
	{
		if (!tank.first->exists())
		{
			BWAPI::Broodwar << "dead tank in squad list" << std::endl;
			removeTank(tank.first);
			continue;
		}

		// First attempt at siege micro
		if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode))
		{
			if (closestTankToTarget != NULL && tank.first != closestTankToTarget && closestTankToTarget->isSieged() && tank.first->getDistance(closestTankToTarget) < 32)
			{
				if (!tank.first->isSieged())
					tank.first->siege();

				tank.second = BWAPI::Broodwar->getFrameCount();
			}

			int targetDistance = 999999;
			BWAPI::Unit closestTargetForTank;
			for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
			{
				if (!enemy)
					continue;

				if (enemy->exists() && tank.first->getDistance(enemy) < targetDistance && !enemy->isFlying())
				{
					targetDistance = tank.first->getDistance(enemy);
					closestTargetForTank = enemy;
				}
			}

			if (targetDistance <= BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange() - 8)
			{
				if (!tank.first->isSieged())
					tank.first->siege();

				tank.second = BWAPI::Broodwar->getFrameCount();
			}

			if (tank.first->isSieged() && BWAPI::Broodwar->getFrameCount() - tank.second > 400)
			{
				tank.first->unsiege();
			}
			else if (!tank.first->isSieged())
			{
				if (haveGathered)
				{
					if (!closeEnough(tank.first->getPosition(), attackPoint))
						tank.first->attack(attackPoint);
				}
				else
				{
					if (!closeEnough(tank.first->getPosition(), forwardGather))
					{
						tank.first->attack(forwardGather);
					}
				}
			}
		}
		else
		{		
			if (!tank.first->isAttacking() && !tank.first->isMoving() && !tank.first->isUnderAttack())
			{
				if (haveGathered)
				{
					if (!closeEnough(tank.first->getPosition(), attackPoint))
						tank.first->attack(attackPoint);
				}
				else
				{
					if (!closeEnough(tank.first->getPosition(), forwardGather))
					{
						tank.first->attack(forwardGather);
					}
				}
			}
		}
	}
	
	
	for (auto vulture : _vultures)
	{
		if (!vulture->exists())
		{
			BWAPI::Broodwar << "dead vulture in squad list" << std::endl;
			removeVulture(vulture);
			continue;
		}


		if (closestTankToTarget != NULL)
		{
			int distanceBetween = 0;

			if (haveGathered)
			{
				int distanceBetween = sqrt(((attackPoint.x - closestTankToTarget->getPosition().x) * (attackPoint.x - closestTankToTarget->getPosition().x)) +
					((attackPoint.y - closestTankToTarget->getPosition().y) * (attackPoint.y - closestTankToTarget->getPosition().y)));
			}
			else
			{
				int distanceBetween = sqrt(((forwardGather.x - closestTankToTarget->getPosition().x) * (forwardGather.x - closestTankToTarget->getPosition().x)) +
					((forwardGather.y - closestTankToTarget->getPosition().y) * (forwardGather.y - closestTankToTarget->getPosition().y)));
			}

			if (distanceBetween > 0)
			{
				int ratio = 64 / distanceBetween;
				int x = closestTankToTarget->getPosition().x + ratio * (attackPoint.x - closestTankToTarget->getPosition().x);
				int y = closestTankToTarget->getPosition().y + ratio * (attackPoint.y - closestTankToTarget->getPosition().y);
				BWAPI::Position buffer = BWAPI::Position(x, y);

				if (!closeEnough(vulture->getPosition(), buffer))
					vulture->attack(buffer);
			}
			else
			{
				if (haveGathered)
				{
					if (!closeEnough(vulture->getPosition(), attackPoint))
						vulture->attack(attackPoint);
				}
				else
				{
					if (!closeEnough(vulture->getPosition(), forwardGather))
					{
						vulture->attack(forwardGather);
					}
				}
			}
		}
		else
		{
			if (!vulture->isAttacking() && !vulture->isMoving() && !vulture->isUnderAttack())
			{
				if (haveGathered)
				{
					if (!closeEnough(vulture->getPosition(), attackPoint))
						vulture->attack(attackPoint);
				}
				else
				{
					if (!closeEnough(vulture->getPosition(), forwardGather))
					{
						vulture->attack(forwardGather);
					}
				}
			}
		}
	}

	for (auto goliath : _goliaths)
	{
		if (!goliath->exists())
		{
			BWAPI::Broodwar << "dead goliath in squad list" << std::endl;
			removeTank(goliath);
			continue;
		}

		if (closestTankToTarget != NULL)
		{
			/*BWEM::CPPath pathToTarget;
			int shortest = 999999;
			int distance = 0;
			int length = -1;
			BWAPI::Position nextChokePos = BWAPI::Position(0, 0);

			bool noChokePath = false;

			if (haveGathered)
				pathToTarget = theMap.GetPath(attackPoint, closestTankToTarget->getPosition(), &length);
			else
				pathToTarget = theMap.GetPath(forwardGather, closestTankToTarget->getPosition(), &length);

			if (length < 0)
			{
				noChokePath = false;
				goto skipCalculation;
			}

			for (auto choke : pathToTarget)
			{
				distance = sqrt(((BWAPI::Position(choke->Center()).x - closestTankToTarget->getPosition().x) * (BWAPI::Position(choke->Center()).x - closestTankToTarget->getPosition().x)) +
					((BWAPI::Position(choke->Center()).y - closestTankToTarget->getPosition().y)) * (BWAPI::Position(choke->Center()).y - closestTankToTarget->getPosition().y));

				if (distance < shortest && distance > 124)
				{
					nextChokePos = BWAPI::Position(BWAPI::Position(choke->Center()).x, BWAPI::Position(choke->Center()).y);
					shortest = distance;
				}
			}
			if (nextChokePos == BWAPI::Position(0, 0))
			{
				noChokePath = false;
				goto skipCalculation;
			}

			int distanceBetween = sqrt(((nextChokePos.x - closestTankToTarget->getPosition().x) * (nextChokePos.x - closestTankToTarget->getPosition().x)) +
				((nextChokePos.y - closestTankToTarget->getPosition().y) * (nextChokePos.y - closestTankToTarget->getPosition().y)));

			int ratio = 64 / distanceBetween;
			int x = closestTankToTarget->getPosition().x + ratio * (nextChokePos.x - closestTankToTarget->getPosition().x);
			int y = closestTankToTarget->getPosition().y + ratio * (nextChokePos.y - closestTankToTarget->getPosition().y);
			BWAPI::Position buffer = BWAPI::Position(x, y);

			if (!closeEnough(goliath->getPosition(), buffer))
				goliath->attack(buffer);

		skipCalculation:
			if (noChokePath)
			{
				if (haveGathered)
				{
					if (!closeEnough(goliath->getPosition(), attackPoint))
						goliath->attack(attackPoint);
				}
				else
				{
					if (!closeEnough(goliath->getPosition(), forwardGather))
					{
						goliath->attack(forwardGather);
					}
				}
			}*/

			int distanceBetween = 0;

			if (haveGathered)
			{
				int distanceBetween = sqrt(((attackPoint.x - closestTankToTarget->getPosition().x) * (attackPoint.x - closestTankToTarget->getPosition().x)) +
					((attackPoint.y - closestTankToTarget->getPosition().y) * (attackPoint.y - closestTankToTarget->getPosition().y)));
			}
			else
			{
				int distanceBetween = sqrt(((forwardGather.x - closestTankToTarget->getPosition().x) * (forwardGather.x - closestTankToTarget->getPosition().x)) +
					((forwardGather.y - closestTankToTarget->getPosition().y) * (forwardGather.y - closestTankToTarget->getPosition().y)));
			}
			if (distanceBetween > 0)
			{
				int ratio = 64 / distanceBetween;
				int x = closestTankToTarget->getPosition().x + ratio * (attackPoint.x - closestTankToTarget->getPosition().x);
				int y = closestTankToTarget->getPosition().y + ratio * (attackPoint.y - closestTankToTarget->getPosition().y);
				BWAPI::Position buffer = BWAPI::Position(x, y);

				if (!closeEnough(goliath->getPosition(), buffer))
					goliath->attack(buffer);
			}
			else
			{
				if (haveGathered)
				{
					if (!closeEnough(goliath->getPosition(), attackPoint))
						goliath->attack(attackPoint);
				}
				else
				{
					if (!closeEnough(goliath->getPosition(), forwardGather))
					{
						goliath->attack(forwardGather);
					}
				}
			}
		}
		else
		{
			if (!goliath->isAttacking() && !goliath->isMoving() && !goliath->isUnderAttack())
			{
				if (haveGathered)
				{
					if (!closeEnough(goliath->getPosition(), attackPoint))
						goliath->attack(attackPoint);
				}
				else
				{
					if (!closeEnough(goliath->getPosition(), forwardGather))
					{
						goliath->attack(forwardGather);
					}
				}
			}
		}
	}
}

void insanitybot::Squad::attack(BWAPI::Unit target)
{
	std::list<BWAPI::Unit> injured;
	injured.clear();
	BWAPI::Position approximateSquadPosition = getSquadPosition();

	/****************************************************************************************
	* Bio
	*****************************************************************************************/
	for (auto marine : _marines)
	{
		if (!marine->exists())
		{
			BWAPI::Broodwar << "dead marine in list" << std::endl;
			removeMarine(marine);
			continue;
		}

		if (!marine->isAttacking() && !marine->isMoving() && !marine->isUnderAttack())
		{
			if (target->exists())
				marine->attack(target);

			if (!closeEnough(marine->getPosition(), target->getInitialPosition()))
			{
				marine->move(target->getInitialPosition());
			}
		}

		if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
		{
			if (marine->isAttacking() && !marine->isStimmed() &&
				(marine->getHitPoints() == marine->getType().maxHitPoints() || _medics.size()))
			{
				marine->useTech(BWAPI::TechTypes::Stim_Packs);
			}
		}

		if (marine->isCompleted() && marine->getHitPoints() < marine->getType().maxHitPoints())
		{
			if (!marine->exists())
				continue;
			injured.push_back(marine);
		}
	}

	int spread = 0;
	for (auto medic : _medics)
	{
		if (injured.size())
		{
			for (auto injuredSquadmate : injured)
			{
				if (!injuredSquadmate->isBeingHealed())
				{
					medic->useTech(BWAPI::TechTypes::Healing, injuredSquadmate);
					break;
				}
			}
		}
		else
		{
			// Effort to prevent medics crowding one marine and blocking movement
			if (_marines.size() > 1)
			{
				std::list<BWAPI::Unit>::iterator buddy = _marines.begin();
				if (spread)
				{
					buddy++;
				}

				spread++;
				medic->move((*buddy)->getPosition());
			}
			else if (_marines.size())
			{
				medic->move(_marines.front()->getPosition());
			}
		}
	}

	/****************************************************************************************
	* Mech
	*****************************************************************************************/
	for (auto vulture : _vultures)
	{
		if (!vulture->exists())
		{
			BWAPI::Broodwar << "dead vulture in squad list" << std::endl;
			removeVulture(vulture);
			continue;
		}

		if (!vulture->isAttacking() && !vulture->isMoving() && !vulture->isUnderAttack())
		{
			if (target->exists())
				vulture->attack(target);

			if (!closeEnough(vulture->getPosition(), target->getInitialPosition()))
			{
				vulture->move(target->getInitialPosition());
			}
		}
	}

	for (auto tank : _tanks)
	{
		if (!tank.first->exists())
		{
			BWAPI::Broodwar << "dead tank in squad list" << std::endl;
			removeTank(tank.first);
			continue;
		}

		if (!tank.first->isAttacking() && !tank.first->isMoving() && !tank.first->isUnderAttack())
		{
			if (target->exists())
				tank.first->attack(target);

			if (!closeEnough(tank.first->getPosition(), target->getInitialPosition()))
			{
				tank.first->move(target->getInitialPosition());
			}
		}
	}

	for (auto goliath : _goliaths)
	{
		if (!goliath->exists())
		{
			BWAPI::Broodwar << "dead goliath in squad list" << std::endl;
			removeTank(goliath);
			continue;
		}

		if (!goliath->isAttacking() && !goliath->isMoving() && !goliath->isUnderAttack())
		{
			if (target->exists())
				goliath->attack(target);

			if (!closeEnough(goliath->getPosition(), target->getInitialPosition()))
			{
				goliath->move(target->getInitialPosition());
			}
		}
	}
}

void insanitybot::Squad::gather(BWAPI::Position gatherPoint)
{
	std::list<BWAPI::Unit> injured;
	injured.clear();

	/****************************************************************************************
	* Bio
	*****************************************************************************************/
	for (auto & marine : _marines)
	{
		if (!marine->exists())
		{
			BWAPI::Broodwar << "dead marine in list" << std::endl;
			removeMarine(marine);
			continue;
		}

		if (marine->isUnderAttack() && !marine->isAttacking())
		{
			marine->attack(marine->getPosition());
		}
		else
		{
			if (!closeEnough(gatherPoint, marine->getPosition()))
				marine->attack(gatherPoint);
		}

		if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
		{
			if (marine->isAttacking() && !marine->isStimmed() &&
				(marine->getHitPoints() == marine->getType().maxHitPoints() || _medics.size()))
			{
				marine->useTech(BWAPI::TechTypes::Stim_Packs);
			}
		}

		if (marine->isCompleted() && marine->getHitPoints() < marine->getType().maxHitPoints())
		{
			injured.push_back(marine);
		}
	}

	for (auto medic : _medics)
	{
		if (injured.size())
		{
			for (auto injuredSquadmate : injured)
			{
				if (!injuredSquadmate->isBeingHealed())
				{
					medic->useTech(BWAPI::TechTypes::Healing, injuredSquadmate);
					break;
				}
			}
		}
		else
		{
			if (!closeEnough(gatherPoint, medic->getPosition()))
				medic->move(gatherPoint);
		}
	}

	/****************************************************************************************
	* Mech
	*****************************************************************************************/
	for (auto tank : _tanks)
	{
		if (!tank.first->exists())
		{
			BWAPI::Broodwar << "dead tank in squad list" << std::endl;
			removeTank(tank.first);
			continue;
		}

		if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode))
		{
			if (closeEnough(gatherPoint, tank.first->getPosition()) && !tank.first->isMoving() && !tank.first->isSieged())
			{
				if (!tank.first->isSieged())
					tank.first->siege();
				tank.second = BWAPI::Broodwar->getFrameCount();
			}
			else if (!closeEnough(gatherPoint, tank.first->getPosition()))
			{
				if (tank.first->isSieged())
					tank.first->unsiege();
				tank.first->attack(gatherPoint);
			}
		}
		else
		{
			if (tank.first->isUnderAttack() && !tank.first->isAttacking())
			{
				tank.first->attack(tank.first->getPosition());
			}
			else
			{
				if (!closeEnough(gatherPoint, tank.first->getPosition()))
					tank.first->attack(gatherPoint);
			}
		}
	}

	for (auto vulture : _vultures)
	{
		if (!vulture->exists())
		{
			BWAPI::Broodwar << "dead vulture in squad list" << std::endl;
			removeVulture(vulture);
			continue;
		}

		if (vulture->isUnderAttack() && !vulture->isAttacking())
		{
			vulture->attack(vulture->getPosition());
		}
		else
		{
			if (!closeEnough(gatherPoint, vulture->getPosition()))
				vulture->attack(gatherPoint);
		}
	}

	for (auto goliath : _goliaths)
	{
		if (!goliath->exists())
		{
			BWAPI::Broodwar << "dead goliath in squad list" << std::endl;
			removeTank(goliath);
			continue;
		}

		if (goliath->isUnderAttack() && !goliath->isAttacking())
		{
			goliath->attack(goliath->getPosition());
		}
		else
		{
			if (!closeEnough(gatherPoint, goliath->getPosition()))
				goliath->attack(gatherPoint);
		}
	}
}

bool insanitybot::Squad::closeEnough(BWAPI::Position location1, BWAPI::Position location2)
{
	// If the coordinates are "close enough", we call it good.
	return abs(location1.x - location2.x) <= 128 && abs(location1.y - location2.y) <= 128;
}
