#include "Squad.h"

namespace { auto & theMap = BWEM::Map::Instance(); }

using namespace insanitybot;

insanitybot::Squad::Squad(BWAPI::Unit unit)
{
	goodToAttack = false;
	haveGathered = false;
	maxSupply = BWAPI::Broodwar->self()->supplyUsed() > 392;

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
			if (!marine || !marine->exists())
				continue;
			xsum += marine->getPosition().x;
			ysum += marine->getPosition().y;
		}
	}

	if (_medics.size())
	{
		for (auto medic : _medics)
		{
			if (!medic || !medic->exists())
				continue;
			xsum += medic->getPosition().x;
			ysum += medic->getPosition().y;
		}
	}

	if (_vultures.size())
	{
		for (auto vulture : _vultures)
		{
			if (!vulture || !vulture->exists())
				continue;
			xsum += vulture->getPosition().x;
			ysum += vulture->getPosition().y;
		}
	}

	if (_tanks.size())
	{
		for (auto tank : _tanks)
		{
			if (!tank.first || !tank.first->exists())
				continue;
			xsum += tank.first->getPosition().x;
			ysum += tank.first->getPosition().y;
		}
	}

	if (_goliaths.size())
	{
		for (auto goliath : _goliaths)
		{
			if (!goliath || !goliath->exists())
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

	if (closeEnough(approximateSquadPosition, forwardGather) || forwardGather == BWAPI::Position(0, 0) || !BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(forwardGather)))
		haveGathered = true;

	if (BWAPI::Broodwar->self()->supplyUsed() > 392)
		maxSupply = true;
	else if (BWAPI::Broodwar->self()->supplyUsed() < 300 && maxSupply)
		maxSupply = false;

	/****************************************************************************************
	* Bio
	*****************************************************************************************/
	for (std::list <BWAPI::Unit>::iterator marine = _marines.begin(); marine != _marines.end();)
	{
		if (!(*marine) || !(*marine)->exists())
		{
			marine = _marines.erase(marine);
		}
		else
		{
			if (!(*marine)->isAttacking() && !(*marine)->isMoving() && !(*marine)->isUnderAttack())
			{
				if (haveGathered)
				{
					if (!closeEnough((*marine)->getPosition(), attackPoint))
						(*marine)->attack(attackPoint);
				}
				else
				{
					if (!closeEnough((*marine)->getPosition(), forwardGather))
					{
						(*marine)->attack(forwardGather);
					}
				}
			}

			if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
			{
				if ((*marine)->isAttacking() && !(*marine)->isStimmed() &&
					((*marine)->getHitPoints() == (*marine)->getType().maxHitPoints() || _medics.size()))
				{
					(*marine)->useTech(BWAPI::TechTypes::Stim_Packs);
				}
			}

			if ((*marine)->isCompleted() && (*marine)->getHitPoints() < (*marine)->getType().maxHitPoints())
			{
				injured.push_back((*marine));
			}

			marine++;
		}
	}

	int spread = 0;
	for (std::list <BWAPI::Unit>::iterator medic = _medics.begin(); medic != _medics.end();)
	{
		if (!(*medic) || !(*medic)->exists())
		{
			medic = _medics.erase(medic);
		}
		else
		{
			if (injured.size())
			{
				for (auto injuredSquadmate : injured)
				{
					if (!injuredSquadmate->isBeingHealed())
					{
						(*medic)->useTech(BWAPI::TechTypes::Healing, injuredSquadmate);
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
					(*medic)->move((*buddy)->getPosition());
				}
				else if (_marines.size())
				{
					(*medic)->move(_marines.front()->getPosition());
				}
			}

			medic++;
		}
	}

	/****************************************************************************************
	* Mech
	*****************************************************************************************/
	int closest = 999999;
	BWAPI::Unit closestTankToTarget = NULL;
	for (std::map<BWAPI::Unit, int>::iterator tank = _tanks.begin(); tank != _tanks.end();)
	{
		if (!tank->first || !tank->first->exists())
		{
			tank = _tanks.erase(tank);
		}
		else
		{
			if (!haveGathered && tank->first->getDistance(forwardGather) < closest)
			{
				closestTankToTarget = tank->first;
				closest = tank->first->getDistance(forwardGather);
			}
			else if (haveGathered && tank->first->getDistance(attackPoint) < closest)
			{
				closestTankToTarget = tank->first;

				closest = tank->first->getDistance(attackPoint);
			}

			tank++;
		}
	}

	for (std::map<BWAPI::Unit, int>::iterator tank = _tanks.begin(); tank != _tanks.end();)
	{
		if (!tank->first || !tank->first->exists())
		{
			tank = _tanks.erase(tank);
		}
		else
		{
			// First attempt at siege micro
			if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode))
			{
				if (closestTankToTarget != NULL && tank->first != closestTankToTarget && closestTankToTarget->isSieged() && tank->first->getDistance(closestTankToTarget) < 32)
				{
					if (!tank->first->isSieged())
						tank->first->siege();

					tank->second = BWAPI::Broodwar->getFrameCount();
				}

				int targetDistance = 999999;
				BWAPI::Unit closestTargetForTank;
				for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
				{
					if (!enemy)
						continue;

					if (enemy->exists() && tank->first->getDistance(enemy) < targetDistance && !enemy->isFlying())
					{
						targetDistance = tank->first->getDistance(enemy);
						closestTargetForTank = enemy;
					}
				}

				if (targetDistance <= BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange() - 8)
				{
					if (!tank->first->isSieged())
						tank->first->siege();

					tank->second = BWAPI::Broodwar->getFrameCount();
				}

				if (tank->first->isSieged() && BWAPI::Broodwar->getFrameCount() - tank->second > 400)
				{
					tank->first->unsiege();
				}
				else if (!tank->first->isSieged())
				{
					if (haveGathered)
					{
						if (!closeEnough(tank->first->getPosition(), attackPoint))
							tank->first->attack(attackPoint);
					}
					else
					{
						if (!closeEnough(tank->first->getPosition(), forwardGather))
						{
							tank->first->attack(forwardGather);
						}
					}
				}
			}
			else
			{
				if (!tank->first->isAttacking() && !tank->first->isMoving() && !tank->first->isUnderAttack())
				{
					if (haveGathered)
					{
						if (!closeEnough(tank->first->getPosition(), attackPoint))
							tank->first->attack(attackPoint);
					}
					else
					{
						if (!closeEnough(tank->first->getPosition(), forwardGather))
						{
							tank->first->attack(forwardGather);
						}
					}
				}
			}

			tank++;
		}
	}
	
	
	for (std::list <BWAPI::Unit>::iterator vulture = _vultures.begin(); vulture != _vultures.end();)
	{
		if (!(*vulture) || !(*vulture)->exists())
		{
			vulture = _vultures.erase(vulture);
		}
		else
		{
			if (closestTankToTarget != NULL)
			{
				if ((*vulture)->getDistance(closestTankToTarget) < 64 || maxSupply)
				{
					(*vulture)->attack(attackPoint);
				}
				else if ((*vulture)->getDistance(closestTankToTarget) > 128)
				{
					(*vulture)->attack(closestTankToTarget->getPosition());
				}
			}
			else
			{
				if (!(*vulture)->isAttacking() && !(*vulture)->isMoving() && !(*vulture)->isUnderAttack())
				{
					if (haveGathered)
					{
						if (!closeEnough((*vulture)->getPosition(), attackPoint))
							(*vulture)->attack(attackPoint);
					}
					else
					{
						if (!closeEnough((*vulture)->getPosition(), forwardGather))
						{
							(*vulture)->attack(forwardGather);
						}
					}
				}
			}

			vulture++;
		}
	}

	for (std::list<BWAPI::Unit>::iterator goliath = _goliaths.begin(); goliath != _goliaths.end();)
	{
		if (!(*goliath) || !(*goliath)->exists())
		{
			goliath = _goliaths.erase(goliath);
		}
		else
		{
			if (closestTankToTarget != NULL)
			{
				if ((*goliath)->getDistance(closestTankToTarget) < 64 || maxSupply)
				{
					(*goliath)->attack(attackPoint);
				}
				else if ((*goliath)->getDistance(closestTankToTarget) > 128)
				{
					(*goliath)->attack(closestTankToTarget->getPosition());
				}
			}
			else
			{
				if (!(*goliath)->isAttacking() && !(*goliath)->isMoving() && !(*goliath)->isUnderAttack())
				{
					if (haveGathered)
					{
						if (!closeEnough((*goliath)->getPosition(), attackPoint))
							(*goliath)->attack(attackPoint);
					}
					else
					{
						if (!closeEnough((*goliath)->getPosition(), forwardGather))
						{
							(*goliath)->attack(forwardGather);
						}
					}
				}
			}

			goliath++;
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
	for (std::list <BWAPI::Unit>::iterator marine = _marines.begin(); marine != _marines.end();)
	{
		if (!(*marine) || !(*marine)->exists())
		{
			marine = _marines.erase(marine);
		}
		else
		{
			if (!(*marine)->isAttacking() && !(*marine)->isMoving() && !(*marine)->isUnderAttack())
			{
				if (target->exists())
					(*marine)->attack(target);

				if (!closeEnough((*marine)->getPosition(), target->getInitialPosition()))
				{
					(*marine)->move(target->getInitialPosition());
				}
			}

			if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
			{
				if ((*marine)->isAttacking() && !(*marine)->isStimmed() &&
					((*marine)->getHitPoints() == (*marine)->getType().maxHitPoints() || _medics.size()))
				{
					(*marine)->useTech(BWAPI::TechTypes::Stim_Packs);
				}
			}

			if ((*marine)->isCompleted() && (*marine)->getHitPoints() < (*marine)->getType().maxHitPoints())
			{
				if (!(*marine)->exists())
					continue;
				injured.push_back((*marine));
			}

			marine++;
		}
	}

	int spread = 0;
	for (std::list <BWAPI::Unit>::iterator medic = _medics.begin(); medic != _medics.end();)
	{
		if (!(*medic) || !(*medic)->exists())
		{
			medic = _medics.erase(medic);
		}
		else
		{
			if (injured.size())
			{
				for (auto injuredSquadmate : injured)
				{
					if (!injuredSquadmate->isBeingHealed())
					{
						(*medic)->useTech(BWAPI::TechTypes::Healing, injuredSquadmate);
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
					(*medic)->move((*buddy)->getPosition());
				}
				else if (_marines.size())
				{
					(*medic)->move(_marines.front()->getPosition());
				}
			}

			medic++;
		}
	}

	/****************************************************************************************
	* Mech
	*****************************************************************************************/
	for (std::list <BWAPI::Unit>::iterator vulture = _vultures.begin(); vulture != _vultures.end();)
	{
		if (!(*vulture) || !(*vulture)->exists())
		{
			vulture = _vultures.erase(vulture);
		}
		else
		{
			if (!(*vulture)->isAttacking() && !(*vulture)->isMoving() && !(*vulture)->isUnderAttack())
			{
				if (target->exists())
					(*vulture)->attack(target);

				if (!closeEnough((*vulture)->getPosition(), target->getInitialPosition()))
				{
					(*vulture)->move(target->getInitialPosition());
				}
			}

			vulture++;
		}
	}

	for (std::map<BWAPI::Unit, int>::iterator tank = _tanks.begin(); tank != _tanks.end();)
	{
		if (!tank->first || !tank->first->exists())
		{
			tank = _tanks.erase(tank);
		}
		else
		{
			if (!tank->first->isAttacking() && !tank->first->isMoving() && !tank->first->isUnderAttack())
			{
				if (target->exists())
					tank->first->attack(target);

				if (!closeEnough(tank->first->getPosition(), target->getInitialPosition()))
				{
					tank->first->move(target->getInitialPosition());
				}
			}

			tank++;
		}
	}

	for (std::list<BWAPI::Unit>::iterator goliath = _goliaths.begin(); goliath != _goliaths.end();)
	{
		if (!(*goliath) || !(*goliath)->exists())
		{
			goliath = _goliaths.erase(goliath);
		}
		else
		{
			if (!(*goliath)->isAttacking() && !(*goliath)->isMoving() && !(*goliath)->isUnderAttack())
			{
				if (target->exists())
					(*goliath)->attack(target);

				if (!closeEnough((*goliath)->getPosition(), target->getInitialPosition()))
				{
					(*goliath)->move(target->getInitialPosition());
				}
			}

			goliath++;
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
	for (std::list <BWAPI::Unit>::iterator marine = _marines.begin(); marine != _marines.end();)
	{
		if (!(*marine) || !(*marine)->exists())
		{
			marine = _marines.erase(marine);
		}
		else
		{
			if ((*marine)->isUnderAttack())
			{
				(*marine)->attack((*marine)->getPosition());
			}
			else
			{
				if (!closeEnough(gatherPoint, (*marine)->getPosition()))
					(*marine)->attack(gatherPoint);
			}

			if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
			{
				if ((*marine)->isAttacking() && !(*marine)->isStimmed() &&
					((*marine)->getHitPoints() == (*marine)->getType().maxHitPoints() || _medics.size()))
				{
					(*marine)->useTech(BWAPI::TechTypes::Stim_Packs);
				}
			}

			if ((*marine)->isCompleted() && (*marine)->getHitPoints() < (*marine)->getType().maxHitPoints())
			{
				injured.push_back((*marine));
			}

			marine++;
		}
	}

	for (std::list <BWAPI::Unit>::iterator medic = _medics.begin(); medic != _medics.end();)
	{
		if (!(*medic) || !(*medic)->exists())
		{
			medic = _medics.erase(medic);
		}
		else
		{
			if (injured.size())
			{
				for (auto injuredSquadmate : injured)
				{
					if (!injuredSquadmate->isBeingHealed())
					{
						(*medic)->useTech(BWAPI::TechTypes::Healing, injuredSquadmate);
						break;
					}
				}
			}
			else
			{
				if (!closeEnough(gatherPoint, (*medic)->getPosition()))
					(*medic)->move(gatherPoint);
			}

			medic++;
		}
	}

	/****************************************************************************************
	* Mech
	*****************************************************************************************/
	for (std::map<BWAPI::Unit, int>::iterator tank = _tanks.begin(); tank != _tanks.end();)
	{
		if (!tank->first || !tank->first->exists())
		{
			tank = _tanks.erase(tank);
		}
		else
		{
			if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode))
			{
				if (closeEnough(gatherPoint, tank->first->getPosition()) && !tank->first->isMoving() && !tank->first->isSieged())
				{
					if (!tank->first->isSieged())
						tank->first->siege();
					tank->second = BWAPI::Broodwar->getFrameCount();
				}
				else if (!closeEnough(gatherPoint, tank->first->getPosition()))
				{
					int targetDistance = 999999;
					BWAPI::Unit closestTargetForTank;
					for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
					{
						if (!enemy)
							continue;

						if (enemy->exists() && tank->first->getDistance(enemy) < targetDistance && !enemy->isFlying())
						{
							targetDistance = tank->first->getDistance(enemy);
							closestTargetForTank = enemy;
						}
					}

					if (targetDistance <= BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange() - 8)
					{
						if (!tank->first->isSieged())
							tank->first->siege();

						tank->second = BWAPI::Broodwar->getFrameCount();
					}

					if (tank->first->isSieged() && BWAPI::Broodwar->getFrameCount() - tank->second > 400)
					{
						tank->first->unsiege();
					}
					else if (!tank->first->isSieged())
					{
						tank->first->attack(gatherPoint);
					}
				}
			}
			else
			{
				if (tank->first->isUnderAttack() && !tank->first->isAttacking())
				{
					tank->first->attack(tank->first->getPosition());
				}
				else
				{
					if (!closeEnough(gatherPoint, tank->first->getPosition()))
						tank->first->attack(gatherPoint);
				}
			}

			tank++;
		}
	}

	for (std::list <BWAPI::Unit>::iterator vulture = _vultures.begin(); vulture != _vultures.end();)
	{
		if (!(*vulture) || !(*vulture)->exists())
		{
			vulture = _vultures.erase(vulture);
		}
		else
		{
			if ((*vulture)->isUnderAttack() && !(*vulture)->isAttacking())
			{
				(*vulture)->attack((*vulture)->getPosition());
			}
			else
			{
				if (!closeEnough(gatherPoint, (*vulture)->getPosition()))
					(*vulture)->attack(gatherPoint);
			}

			vulture++;
		}
	}

	for (std::list<BWAPI::Unit>::iterator goliath = _goliaths.begin(); goliath != _goliaths.end();)
	{
		if (!(*goliath) || !(*goliath)->exists())
		{
			goliath = _goliaths.erase(goliath);
		}
		else
		{
			if ((*goliath)->isUnderAttack() && !(*goliath)->isAttacking())
			{
				(*goliath)->attack((*goliath)->getPosition());
			}
			else
			{
				if (!closeEnough(gatherPoint, (*goliath)->getPosition()))
					(*goliath)->attack(gatherPoint);
			}

			goliath++;
		}
	}
}

bool insanitybot::Squad::closeEnough(BWAPI::Position location1, BWAPI::Position location2)
{
	// If the coordinates are "close enough", we call it good.
	return abs(location1.x - location2.x) <= 128 && abs(location1.y - location2.y) <= 128;
}
