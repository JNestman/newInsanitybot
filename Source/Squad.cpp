#include "Squad.h"
#define _USE_MATH_DEFINES
#include <math.h>

namespace { auto & theMap = BWEM::Map::Instance(); }

using namespace insanitybot;

insanitybot::Squad::Squad(BWAPI::Unit unit, bool isAllIn)
{
	goodToAttack = false;
	haveGathered = false;
	if (isAllIn)
		maxSupply = 300;
	else
		maxSupply = 392;

	selfPreservationToTheWind = false;

	_marines.clear();
	_medics.clear();
	_ghosts.clear();
	_vultures.clear();
	_tanks.clear();
	_goliaths.clear();

	nuker = NULL;

	if (unit->getType() == BWAPI::UnitTypes::Terran_Marine)
	{
		_marines.push_back(unit);
	}
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Medic)
	{
		_medics.push_back(unit);
	}
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Ghost)
	{
		_ghosts.push_back(unit);
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
	if (infantrySquadSize() + mechSquadSize() + specialistSquadSize() == 0)
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

	if (_ghosts.size())
	{
		for (auto ghost : _ghosts)
		{
			if (!ghost || !ghost->exists())
				continue;
			xsum += ghost->getPosition().x;
			ysum += ghost->getPosition().y;
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

	return BWAPI::Position(xsum / (infantrySquadSize() + mechSquadSize() + specialistSquadSize()), ysum / (infantrySquadSize() + mechSquadSize() + specialistSquadSize()));
}

/************************************************************************************
* Attack the next point of interest
*************************************************************************************/
void insanitybot::Squad::attack(BWAPI::Position attackPoint, BWAPI::Position forwardGather, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD)
{
	std::list<BWAPI::Unit> injured;
	injured.clear();

	BWAPI::Unitset enemyUnits = BWAPI::Broodwar->enemy()->getUnits();

	BWAPI::Position approximateSquadPosition = getSquadPosition();

	if (closeEnough(approximateSquadPosition, forwardGather) || forwardGather == BWAPI::Position(0, 0) || !BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(forwardGather)))
		haveGathered = true;

	if (BWAPI::Broodwar->self()->supplyUsed() > maxSupply)
		selfPreservationToTheWind = true;
	else if (BWAPI::Broodwar->self()->supplyUsed() < maxSupply - 100)
		selfPreservationToTheWind = false;

	// Storm dodging attempt
	std::list<BWAPI::Bullet> _activePsiStorms;
	_activePsiStorms.clear();

	for (auto bullet : BWAPI::Broodwar->getBullets())
	{
		if (bullet->getType() == BWAPI::BulletTypes::Psionic_Storm)
		{
			_activePsiStorms.push_back(bullet);
		}
	}

	/****************************************************************************************
	* Bio
	*****************************************************************************************/
	for (std::list <BWAPI::Unit>::iterator & marine = _marines.begin(); marine != _marines.end();)
	{
		if (!(*marine) || !(*marine)->exists())
		{
			marine = _marines.erase(marine);
		}
		else
		{
			if (_activePsiStorms.size())
			{
				for (auto storm : _activePsiStorms)
				{
					if ((*marine)->getDistance(storm->getPosition()) < 100)
					{
						(*marine)->move(stormDodge((*marine)->getPosition(), storm->getPosition()));
						continue;
					}
				}
			}

			if ((*marine)->getLastCommandFrame() == BWAPI::Broodwar->getFrameCount())
			{
				marine++;
				continue;
			}

			int closestEnemy = 9999999;
			BWAPI::Position enemyGuy = BWAPI::Position(0, 0);
			for (auto enemy : enemyUnits)
			{
				if (!enemy)
					continue;

				if (enemy->exists() && (*marine)->getDistance(enemy) < closestEnemy)
				{
					closestEnemy = (*marine)->getDistance(enemy);
					enemyGuy = enemy->getPosition();
				}
			}


			int closest = 999999;
			BWAPI::Unit closestTankToTarget = NULL;
			if (_tanks.size())
			{
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
			}

			if (closestTankToTarget != NULL)
			{
				if (((*marine)->getDistance(closestTankToTarget) < 64 || isMaxSupply()) &&
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8))
				{
					(*marine)->attack(attackPoint);
				}
				else if ((*marine)->getDistance(closestTankToTarget) > 128 &&
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8))
				{
					(*marine)->attack(closestTankToTarget->getPosition());
				}
			}
			else
			{
				if ((!(*marine)->isAttacking() && !(*marine)->isMoving() && !(*marine)->isUnderAttack()) ||
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8))
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
			}

			/*if ((*marine)->getGroundWeaponCooldown() > 0 && !(*marine)->isAttackFrame() && !(*marine)->isAttacking() && !(*marine)->isStartingAttack())
			{
				if (!(*marine)->isMoving())
					groundKiteMicro((*marine), enemyGuy);
			}
			else if ((!(*marine)->isAttacking() && (*marine)->getOrder() != BWAPI::Orders::AttackMove && 
				!(*marine)->isAttackFrame() && !(*marine)->isStartingAttack()) ||
				(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8))
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
			else if ((!(*marine)->isAttacking() && (*marine)->getOrder() != BWAPI::Orders::AttackMove && 
				!(*marine)->isAttackFrame() && !(*marine)->isStartingAttack()) &&
				(closestEnemy < BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8))
			{
				(*marine)->attack(enemyGuy);
			}*/

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
			if (flareTarget((*medic), _flareBD))
			{
				
			}
			else if (injured.size())
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
					if (spread < _marines.size())
					{
						for (int x = 0; x < spread; x++)
						{
							buddy++;
						}
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
				for (auto enemy : enemyUnits)
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
				if ((*vulture)->getDistance(closestTankToTarget) < 64 || isMaxSupply())
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
				if ((*goliath)->getDistance(closestTankToTarget) < 64 || isMaxSupply())
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

	/************************************************************************************
	* Specialist Squad
	*************************************************************************************/
	for (std::list <BWAPI::Unit>::iterator ghost = _ghosts.begin(); ghost != _ghosts.end();)
	{
		if (!(*ghost) || !(*ghost)->exists())
		{
			if ((*ghost) == nuker)
				nuker = NULL;

			ghost = _ghosts.erase(ghost);
		}
		else if ((*ghost) == nuker)
		{
			ghost++;
		}
		else
		{
			int closestEnemy = 9999999;
			for (auto enemy : enemyUnits)
			{
				if (!enemy)
					continue;

				if (enemy->exists() && (*ghost)->getDistance(enemy) < closestEnemy)
				{
					closestEnemy = (*ghost)->getDistance(enemy);
				}
			}

			// save energy
			if ((*ghost)->isCloaked())
			{
				(*ghost)->decloak();
			}
			else if (haveGathered)
			{
				if (!closeEnough((*ghost)->getPosition(), attackPoint) && 
					(!(*ghost)->isAttacking() && !(*ghost)->isMoving() && !(*ghost)->isUnderAttack()) ||
					(closestEnemy > BWAPI::UnitTypes::Terran_Ghost.groundWeapon().maxRange() + 8))
				{
					(*ghost)->attack(attackPoint);
				}
			}
			else
			{
				if (!closeEnough((*ghost)->getPosition(), forwardGather) &&
					(!(*ghost)->isAttacking() && !(*ghost)->isMoving() && !(*ghost)->isUnderAttack()) ||
					(closestEnemy > BWAPI::UnitTypes::Terran_Ghost.groundWeapon().maxRange() + 8))
				{
					(*ghost)->attack(forwardGather);
				}
			}

			ghost++;
		}
	}
}

/****************************************************************************************
* Attack a neutral structure
*****************************************************************************************/
void insanitybot::Squad::attack(BWAPI::Unit target, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD)
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
			if (flareTarget((*medic), _flareBD))
			{
				
			}
			else if (injured.size())
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

	/****************************************************************************************
	* Specialists
	*****************************************************************************************/
	for (std::list <BWAPI::Unit>::iterator ghost = _ghosts.begin(); ghost != _ghosts.end();)
	{
		if (!(*ghost) || !(*ghost)->exists())
		{
			ghost = _ghosts.erase(ghost);
		}
		else
		{
			if ((*ghost)->isCloaked())
			{
				(*ghost)->decloak();
			}
			else if (!(*ghost)->isAttacking() && !(*ghost)->isMoving() && !(*ghost)->isUnderAttack())
			{
				if (target->exists())
					(*ghost)->attack(target);

				if (!closeEnough((*ghost)->getPosition(), target->getInitialPosition()))
				{
					(*ghost)->move(target->getInitialPosition());
				}
			}

			ghost++;
		}
	}
}

void insanitybot::Squad::gather(BWAPI::Position gatherPoint, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD)
{
	BWAPI::Unitset enemyUnits = BWAPI::Broodwar->enemy()->getUnits();

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
			if (!closeEnough(gatherPoint, (*marine)->getPosition()))
					(*marine)->attack(gatherPoint);

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
			if (flareTarget((*medic), _flareBD))
			{

			}
			else if (injured.size())
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
				if (!closeEnough(gatherPoint, tank->first->getPosition()))
						tank->first->attack(gatherPoint);
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
			if (!closeEnough(gatherPoint, (*vulture)->getPosition()))
					(*vulture)->attack(gatherPoint);

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
			if (!closeEnough(gatherPoint, (*goliath)->getPosition()))
				(*goliath)->attack(gatherPoint);

			goliath++;
		}
	}

	/****************************************************************************************
	* Specialists
	*****************************************************************************************/
	for (std::list <BWAPI::Unit>::iterator ghost = _ghosts.begin(); ghost != _ghosts.end();)
	{
		if (!(*ghost) || !(*ghost)->exists())
		{
			ghost = _marines.erase(ghost);
		}
		else
		{
			if (!closeEnough(gatherPoint, (*ghost)->getPosition()) && 
				!(*ghost)->isMoving() && !(*ghost)->isAttacking() && !(*ghost)->isAttackFrame())
				(*ghost)->attack(gatherPoint);

			ghost++;
		}
	}
}

/***************************************************************
* Special squads that protect our outposts will be handled here.
****************************************************************/
void insanitybot::Squad::protect()
{
	std::list<BWAPI::Unit> injured;
	injured.clear();
	BWAPI::Position defenceTarget = BWAPI::Position(0, 0);

	for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (!enemy)
			continue;

		if (enemy->exists() && enemy->getDistance(BWAPI::Position(frontierLocation)) < 400 && enemy->getType() != BWAPI::UnitTypes::Zerg_Overlord &&
			enemy->getType() != BWAPI::UnitTypes::Protoss_Observer)
			defenceTarget = enemy->getPosition();
	}

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
			if (defenceTarget != BWAPI::Position(0, 0))
			{
				int closestEnemy = 9999999;
				for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
				{
					if (!enemy)
						continue;

					if (enemy->exists() && (*marine)->getDistance(enemy) < closestEnemy)
					{
						closestEnemy = (*marine)->getDistance(enemy);
					}
				}

				if ((!(*marine)->isAttacking() && !(*marine)->isMoving() && !(*marine)->isUnderAttack()) ||
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8))
				{
					(*marine)->attack(BWAPI::Position(defenceTarget));
				}
			}
			else if (!closeEnough(BWAPI::Position(frontierLocation), (*marine)->getPosition()) &&
				!(*marine)->isAttacking() && !(*marine)->isMoving())
			{
				(*marine)->attack(BWAPI::Position(frontierLocation));
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
				if (defenceTarget != BWAPI::Position(0, 0))
				{
					(*medic)->attack(BWAPI::Position(defenceTarget));
				}
				else if (!closeEnough(BWAPI::Position(frontierLocation), (*medic)->getPosition()))
				{
					(*medic)->move(BWAPI::Position(frontierLocation));
				}
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
				if (closeEnough(BWAPI::Position(frontierLocation), tank->first->getPosition()) && !tank->first->isMoving() && !tank->first->isSieged())
				{
					if (!tank->first->isSieged())
						tank->first->siege();
					tank->second = BWAPI::Broodwar->getFrameCount();
				}
				else if (!closeEnough(BWAPI::Position(frontierLocation), tank->first->getPosition()))
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
						tank->first->attack(BWAPI::Position(frontierLocation));
					}
				}
			}
			else
			{
				if (!closeEnough(BWAPI::Position(frontierLocation), tank->first->getPosition()))
					tank->first->attack(BWAPI::Position(frontierLocation));
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
			if (defenceTarget != BWAPI::Position(0, 0))
			{
				(*vulture)->attack(BWAPI::Position(defenceTarget));
			}
			if (!closeEnough(BWAPI::Position(frontierLocation), (*vulture)->getPosition()))
			{
				(*vulture)->attack(BWAPI::Position(frontierLocation));
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
			if (defenceTarget != BWAPI::Position(0, 0))
			{
				(*goliath)->attack(BWAPI::Position(defenceTarget));
			}
			else if (!closeEnough(BWAPI::Position(frontierLocation), (*goliath)->getPosition()))
			{
				(*goliath)->attack(BWAPI::Position(frontierLocation));
			}

			goliath++;
		}
	}
}

/***************************************************************
* If we're doing that nuke thing, handle it here.
****************************************************************/
void insanitybot::Squad::handleNuker(BWAPI::Position target)
{
	if (!nuker || !nuker->exists())
		return;

	int distanceToTarget = nuker->getDistance(target);

	for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
	{
		if ((enemy->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
			enemy->getType() == BWAPI::UnitTypes::Zerg_Spore_Colony ||
			enemy->getType() == BWAPI::UnitTypes::Zerg_Hatchery) &&
			nuker->getDistance(enemy) < distanceToTarget)
		{
			target = enemy->getPosition();
			distanceToTarget = nuker->getDistance(enemy);
		}
	}

	if (nuker->canCloak() && !nuker->isCloaked() && nuker->getEnergy() > 75 &&
		distanceToTarget < 800)
	{
		nuker->cloak();
		return;
	}
	
	nuker->useTech(BWAPI::TechTypes::Nuclear_Strike, target);
}

void insanitybot::Squad::setNuker()
{
	if (nuker || !BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Personnel_Cloaking))
		return;
		

	for (auto & ghost : _ghosts)
	{
		if (!ghost || !ghost->exists())
			continue;

		if (ghost->getEnergy() > 80)
		{
			nuker = ghost;
			break;
		}
	}
}

/***************************************************************
* Is the squad too spread out in a conga line? Mostly for Bio
****************************************************************/
bool insanitybot::Squad::tooSpreadOut()
{
	int tooFar = 250;

	if (_marines.size() > 1)
	{
		for (auto marine : _marines)
		{
			for (auto secondMarine : _marines)
			{
				if (marine != secondMarine && marine->getDistance(secondMarine) > tooFar)
				{
					return true;
				}
			}
		}
	}

	return false;
}

/***************************************************************
* Lets start acting like a micro bot
* Update: Horrible. lol We'll come back to this
****************************************************************/
void insanitybot::Squad::groundKiteMicro(BWAPI::Unit & friendly, BWAPI::Position enemy)
{
	BWAPI::Broodwar->setLocalSpeed(15);

	if (enemy == BWAPI::Position(0, 0))
		return;

	float length = sqrt(pow((enemy.x - friendly->getPosition().x), 2) + pow((enemy.y - friendly->getPosition().y), 2));

	float dx = (enemy.x - friendly->getPosition().x) / length;
	float dy = (enemy.y - friendly->getPosition().y) / length;

	float x = friendly->getPosition().x + 50 * dx;
	float y = friendly->getPosition().y + 50 * dy;

	if (x < 0)
		x = 0;
	else if (x > BWAPI::Broodwar->mapWidth())
		x = BWAPI::Broodwar->mapWidth();

	if (y < 0)
		y = 0;
	else if (y > BWAPI::Broodwar->mapHeight())
		y = BWAPI::Broodwar->mapHeight();

	BWAPI::Position kiteTo = BWAPI::Position(x, y);

	friendly->move(kiteTo);
}

// Storm dodging
BWAPI::Position insanitybot::Squad::stormDodge(BWAPI::Position friendly, BWAPI::Position enemy)
{
	float radius = 100;
	float dx = friendly.x - enemy.x;
	float dy = enemy.y - friendly.y;

	float angle = atan2(dy, dx);

	if (angle < 0)
		angle += 360;

	float radians = angle * (M_PI / 180);

	int x = enemy.x + radius * cos(radians);
	int y = enemy.y + radius * sin(radians);

	return BWAPI::Position(x, y);
}
/***************************************************************
* If the coordinates are "close enough", we call it good.
****************************************************************/
bool insanitybot::Squad::closeEnough(BWAPI::Position location1, BWAPI::Position location2)
{
	return abs(location1.x - location2.x) <= 128 && abs(location1.y - location2.y) <= 128;
}

/***************************************************************
* Will return true if we found a valid target for our medic
* to flare
****************************************************************/
bool insanitybot::Squad::flareTarget(BWAPI::Unit medic, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD)
{
	if (!medic || !medic->exists())
		return false;

	if (medic->getEnergy() < 75 || !BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Optical_Flare))
		return false;

	// Check if the medic already has a target
	for (std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>::iterator target = _flareBD.begin(); target != _flareBD.end(); target++)
	{
		if (target->first == medic)
		{
			medic->useTech(BWAPI::TechTypes::Optical_Flare, target->second.first);
			return true;
		}
	}

	BWAPI::Unit target = NULL;
	int closestDistance = 1000;

	for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (!enemy || !enemy->exists())
			continue;

		// Make sure it is a valid target
		if (!enemy->getType().isBuilding() && enemy->getType().isDetector() && 
			!enemy->isIrradiated() && !enemy->isInvincible() && !enemy->isStasised() && 
			!enemy->isBlind() && notInFlareDB(enemy, _flareBD))
		{
			if (medic->getDistance(enemy) < closestDistance)
			{
				target = enemy;
				closestDistance = medic->getDistance(enemy);
			}
		}
	}

	if (target)
	{
		medic->useTech(BWAPI::TechTypes::Irradiate, target);
		_flareBD.insert(std::pair<BWAPI::Unit, std::pair<BWAPI::Unit, int>>(medic, std::pair<BWAPI::Unit, int>(target, BWAPI::Broodwar->getFrameCount())));
	}
	return target;
}

// Simple check if we've potentially already marked the target to be irradiated
bool insanitybot::Squad::notInFlareDB(BWAPI::Unit potentialTarget, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD)
{
	for (auto target : _flareBD)
	{
		if (target.second.first == potentialTarget)
		{
			return false;
		}
	}
	return true;
}


