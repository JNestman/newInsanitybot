#include "Squad.h"
#define _USE_MATH_DEFINES
#include <math.h>

namespace { auto & theMap = BWEM::Map::Instance(); }

using namespace insanitybot;

insanitybot::Squad::Squad(BWAPI::Unit unit, bool isAllIn)
{
	goodToAttack = false;
	haveGathered = false;
	isAllInSquad = isAllIn;
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
	_bcs.clear();

	nuker = NULL;
	dropship = NULL;

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
		_vultures.insert(std::pair<BWAPI::Unit, int>(unit, 0));
	}
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode || unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
	{
		_tanks.insert(std::pair<BWAPI::Unit, int>(unit, 0));
	}
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Goliath)
	{
		_goliaths.push_back(unit);
	}
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Dropship)
	{
		dropship = unit;
	}
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Battlecruiser)
	{
		_bcs.insert(std::pair<BWAPI::Unit, int>(unit, 0));
	}

	defenseLastNeededFrame = 0;

	mineOffset = 50;
	tankMineOffset = 75;

	dropTarget = BWAPI::Position(0, 0);
}

BWAPI::Position insanitybot::Squad::getSquadPosition()
{
	int totalSquadSize = infantrySquadSize() + mechSquadSize() + specialistSquadSize() + bcSquadSize();
	if (totalSquadSize == 0)
	{
		return BWAPI::Position(1500, 1500);
	}

	int xsum = 0;
	int ysum = 0;

	std::vector<std::list <BWAPI::Unit>> unitLists = { _marines, _medics, _ghosts, _goliaths };
	std::vector<std::map <BWAPI::Unit, int>> unitMaps = { _vultures, _tanks, _bcs };

	for (int i = 0; i < unitLists.size(); i++)
	{
		if (unitLists[i].size())
		{
			for (auto unit : unitLists[i])
			{
				if (!unit || !unit->exists())
					continue;
				xsum += unit->getPosition().x;
				ysum += unit->getPosition().y;
			}
		}
	}

	for (int i = 0; i < unitMaps.size(); i++)
	{
		if (unitMaps[i].size())
		{
			for (auto unit : unitMaps[i])
			{
				if (!unit.first || !unit.first->exists())
					continue;
				xsum += unit.first->getPosition().x;
				ysum += unit.first->getPosition().y;
			}
		}
	}

	return BWAPI::Position(xsum / totalSquadSize, ysum / totalSquadSize);
}

/************************************************************************************
* Attack the next point of interest
*************************************************************************************/
void insanitybot::Squad::attack(BWAPI::Position attackPoint, BWAPI::Position forwardGather, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD, int numEnemyBases)
{
	std::list<BWAPI::Unit> injured;
	injured.clear();

	BWAPI::Unitset enemyUnits = BWAPI::Broodwar->enemy()->getUnits();

	BWAPI::Position approximateSquadPosition = getSquadPosition();

	if (closeEnough(approximateSquadPosition, forwardGather) || forwardGather == BWAPI::Position(0, 0) || 
		(!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(forwardGather)) && !_bcs.size()))
		haveGathered = true;

	if (BWAPI::Broodwar->self()->supplyUsed() > maxSupply)
		selfPreservationToTheWind = true;
	else if (BWAPI::Broodwar->self()->supplyUsed() < maxSupply - 100)
		selfPreservationToTheWind = false;

	// Storm dodging attempt
	std::list<BWAPI::Bullet> _activePsiStorms;
	_activePsiStorms = getPsiStorms();

	// Scarab dodging attempt
	std::list<BWAPI::Unit> _activeScarabs;
	_activeScarabs = getScarabs();

	/****************************************************************************************
	* Bio
	*****************************************************************************************/
	handleMarines(attackPoint, forwardGather, haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, NULL, BWAPI::Position(0,0));
	handleFirebats(attackPoint, forwardGather, haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, NULL, BWAPI::Position(0, 0));
	handleMedics(_flareBD, injured, _activePsiStorms, _activeScarabs, BWAPI::Position(0, 0));

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
	
	
	for (std::map<BWAPI::Unit, int>::iterator vulture = _vultures.begin(); vulture != _vultures.end();)
	{
		if (!vulture->first || !vulture->first->exists())
		{
			vulture = _vultures.erase(vulture);
		}
		else
		{
			if (vulture->second > 1)
			{
				if (vulture->first->getOrder() != BWAPI::Orders::PlaceMine || vulture->second + 100 < BWAPI::Broodwar->getFrameCount())
				{
					vulture->second = 1;
				}
				else
				{
					vulture++;

					continue;
				}
			}

			if (canPlantMine(vulture->first) && shouldPlantMine(vulture->first))
			{
				vulture->second = BWAPI::Broodwar->getFrameCount();

				vulture->first->useTech(BWAPI::TechTypes::Spider_Mines, vulture->first->getPosition());

				vulture++;

				continue;
			}

			if (closestTankToTarget != NULL)
			{
				if (vulture->first->getDistance(closestTankToTarget) < 64 || isMaxSupply())
				{
					if (!closeEnough(vulture->first->getPosition(), attackPoint))
							vulture->first->attack(attackPoint);
				}
				else if (vulture->first->getDistance(closestTankToTarget) > 128)
				{
					vulture->first->attack(closestTankToTarget->getPosition());
				}
			}
			else
			{
				if ((!vulture->first->isAttacking() && !vulture->first->isMoving() && !vulture->first->isUnderAttack())
					|| isAllInSquad)
				{
					if (haveGathered)
					{
						if (!closeEnough(vulture->first->getPosition(), attackPoint))
							vulture->first->attack(attackPoint);
					}
					else
					{
						if (!closeEnough(vulture->first->getPosition(), forwardGather))
							vulture->first->attack(forwardGather);
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
			BWAPI::Unit floatingBuilding = NULL;
			if (!numEnemyBases)
			{
				for (auto enemyUnit : BWAPI::Broodwar->enemy()->getUnits())
				{
					if (!enemyUnit || !enemyUnit->exists())
						continue;

					if (enemyUnit->getType().isBuilding() && enemyUnit->isFlying())
					{
						floatingBuilding = enemyUnit;
						break;
					}
				}
			}

			if (floatingBuilding != NULL)
			{
				if (!closeEnough((*goliath)->getPosition(), floatingBuilding->getPosition()))
					(*goliath)->attack(floatingBuilding->getPosition());
			}
			else
			{
				if (closestTankToTarget != NULL)
				{
					if ((*goliath)->getDistance(closestTankToTarget) < 64 || isMaxSupply())
					{
						if (!closeEnough((*goliath)->getPosition(), attackPoint))
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

	/****************************************************************************************
	* Air
	*****************************************************************************************/
	handleBCs(attackPoint, forwardGather, haveGathered, enemyUnits, NULL, BWAPI::Position(0, 0));

}

/****************************************************************************************
* Attack a neutral structure
*****************************************************************************************/
void insanitybot::Squad::attack(BWAPI::Unit target, std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD)
{
	std::list<BWAPI::Unit> injured;
	injured.clear();
	BWAPI::Unitset enemyUnits = BWAPI::Broodwar->enemy()->getUnits();
	BWAPI::Position approximateSquadPosition = getSquadPosition();

	// Storm dodging attempt
	std::list<BWAPI::Bullet> _activePsiStorms;
	_activePsiStorms = getPsiStorms();

	// Scarab dodging attempt
	std::list<BWAPI::Unit> _activeScarabs;
	_activeScarabs = getScarabs();

	/****************************************************************************************
	* Bio
	*****************************************************************************************/
	handleMarines(BWAPI::Position(0, 0), BWAPI::Position(0, 0), haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, target, BWAPI::Position(0,0));
	handleFirebats(BWAPI::Position(0, 0), BWAPI::Position(0, 0), haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, target, BWAPI::Position(0, 0));
	handleMedics(_flareBD, injured, _activePsiStorms, _activeScarabs, BWAPI::Position(0,0));

	/****************************************************************************************
	* Mech
	*****************************************************************************************/
	for (std::map<BWAPI::Unit, int>::iterator vulture = _vultures.begin(); vulture != _vultures.end();)
	{
		if (!vulture->first || !vulture->first->exists())
		{
			vulture = _vultures.erase(vulture);
		}
		else
		{
			if (!vulture->first->isAttacking() && !vulture->first->isMoving() && !vulture->first->isUnderAttack())
			{
				if (target->exists())
					vulture->first->attack(target);

				if (!closeEnough(vulture->first->getPosition(), target->getInitialPosition()))
				{
					vulture->first->move(target->getInitialPosition());
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
	BWAPI::Position approximateSquadPosition = getSquadPosition();
	std::list<BWAPI::Unit> injured;
	injured.clear();

	// Storm dodging attempt
	std::list<BWAPI::Bullet> _activePsiStorms;
	_activePsiStorms = getPsiStorms();

	// Scarab dodging attempt
	std::list<BWAPI::Unit> _activeScarabs;
	_activeScarabs = getScarabs();

	/****************************************************************************************
	* Bio
	*****************************************************************************************/
	handleMarines(BWAPI::Position(0, 0), BWAPI::Position(0, 0), haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, NULL, gatherPoint);
	handleFirebats(BWAPI::Position(0, 0), BWAPI::Position(0, 0), haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, NULL, gatherPoint);
	handleMedics(_flareBD, injured, _activePsiStorms, _activeScarabs, gatherPoint);

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

	for (std::map<BWAPI::Unit, int>::iterator vulture = _vultures.begin(); vulture != _vultures.end();)
	{
		if (!vulture->first || !vulture->first->exists())
		{
			vulture = _vultures.erase(vulture);
		}
		else
		{
			if (!closeEnough(gatherPoint, vulture->first->getPosition()))
				vulture->first->attack(gatherPoint);

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
			if ((*ghost) == nuker)
				nuker = NULL;

			ghost = _ghosts.erase(ghost);
		}
		else
		{
			if (!closeEnough(gatherPoint, (*ghost)->getPosition()) && 
				!(*ghost)->isMoving() && !(*ghost)->isAttacking() && !(*ghost)->isAttackFrame())
				(*ghost)->attack(gatherPoint);

			ghost++;
		}
	}

	/****************************************************************************************
	* Air
	*****************************************************************************************/
	handleBCs(BWAPI::Position(0, 0), BWAPI::Position(0, 0), haveGathered, enemyUnits, NULL, gatherPoint);
}

/***************************************************************
* Special squads that protect our outposts will be handled here.
****************************************************************/
void insanitybot::Squad::protect()
{
	std::list<BWAPI::Unit> injured;
	injured.clear();
	BWAPI::Position defenceTarget = BWAPI::Position(0, 0);
	BWAPI::Unit friendlyBlockingMine = NULL;

	for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (!enemy)
			continue;

		if (enemy->exists() && enemy->getDistance(BWAPI::Position(frontierLocation)) < 400 && enemy->getType() != BWAPI::UnitTypes::Zerg_Overlord &&
			enemy->getType() != BWAPI::UnitTypes::Protoss_Observer)
			defenceTarget = enemy->getPosition();
	}

	for (auto unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (!unit || !unit->exists())
			continue;

		if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine && unit->getDistance(defenceTarget) < 36)
		{
			friendlyBlockingMine = unit;
			break;
		}
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
			else if (friendlyBlockingMine && !(*marine)->isAttacking() && !(*marine)->isMoving())
			{
				if (closeEnough(friendlyBlockingMine->getPosition(), (*marine)->getPosition()))
				{
					(*marine)->attack(friendlyBlockingMine);
				}
				else
				{
					(*marine)->attack(friendlyBlockingMine->getPosition());
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

	for (std::map<BWAPI::Unit, int>::iterator vulture = _vultures.begin(); vulture != _vultures.end();)
	{
		if (!vulture->first || !vulture->first->exists())
		{
			vulture = _vultures.erase(vulture);
		}
		else
		{
			if (defenceTarget != BWAPI::Position(0, 0) && vulture->first->isIdle())
			{
				vulture->first->attack(BWAPI::Position(defenceTarget));
			}
			else if (friendlyBlockingMine && !vulture->first->isAttacking() && !vulture->first->isMoving())
			{
				if (closeEnough(friendlyBlockingMine->getPosition(), vulture->first->getPosition()))
				{
					vulture->first->attack(friendlyBlockingMine);
				}
				else
				{
					vulture->first->attack(friendlyBlockingMine->getPosition());
				}

			}
			else if (!closeEnough(BWAPI::Position(frontierLocation), vulture->first->getPosition()))
			{
				vulture->first->attack(BWAPI::Position(frontierLocation));
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
			if (defenceTarget != BWAPI::Position(0, 0) && (*goliath)->isIdle())
			{
				(*goliath)->attack(BWAPI::Position(defenceTarget));
			}
			else if (friendlyBlockingMine && !(*goliath)->isAttacking() && !(*goliath)->isMoving())
			{
				if (closeEnough(friendlyBlockingMine->getPosition(), (*goliath)->getPosition()))
				{
					(*goliath)->attack(friendlyBlockingMine);
				}
				else
				{
					(*goliath)->attack(friendlyBlockingMine->getPosition());
				}

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
* Small squad that drops into enemy mineral lines for massive/
* no damage.
****************************************************************/
void insanitybot::Squad::loadDrop(BWAPI::Unit dropship)
{
	if (!dropship || !dropship->exists())
		return;

	if (_marines.size())
	{
		for (std::list <BWAPI::Unit>::iterator marine = _marines.begin(); marine != _marines.end();)
		{
			if (!(*marine) || !(*marine)->exists())
			{
				marine = _marines.erase(marine);
			}
			else
			{
				if (!(*marine)->isLoaded())
					(*marine)->load(dropship);

				marine++;
			}
		}
	}

	if (_medics.size())
	{
		for (std::list <BWAPI::Unit>::iterator medic = _medics.begin(); medic != _medics.end();)
		{
			if (!(*medic) || !(*medic)->exists())
			{
				medic = _medics.erase(medic);
			}
			else
			{
				if (!(*medic)->isLoaded())
					(*medic)->load(dropship);

				medic++;
			}
		}
	}

	if (_vultures.size())
	{
		for (std::map<BWAPI::Unit, int>::iterator vulture = _vultures.begin(); vulture != _vultures.end();)
		{
			if (!vulture->first || !vulture->first->exists())
			{
				vulture = _vultures.erase(vulture);
			}
			else
			{
				if (!vulture->first->isLoaded())
					vulture->first->load(dropship);

				vulture++;
			}
		}
	}

	if (_goliaths.size())
	{
		for (std::list <BWAPI::Unit>::iterator goliath = _goliaths.begin(); goliath != _goliaths.end();)
		{
			if (!(*goliath) || !(*goliath)->exists())
			{
				goliath = _goliaths.erase(goliath);
			}
			else
			{
				if (!(*goliath)->isLoaded())
					(*goliath)->load(dropship);

				goliath++;
			}
		}
	}

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
				if (!tank->first->isLoaded() && !tank->first->isSieged())
					tank->first->load(dropship);
				else if (tank->first->isSieged())
					tank->first->unsiege();

				tank++;
			}
		}
	}
}

void insanitybot::Squad::drop(BWAPI::Unitset enemyUnits)
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
			if ((*marine)->isLoaded())
			{
				marine++;
				continue;
			}
			else
			{
				int closestEnemy = 9999999;
				BWAPI::Position enemyGuy = BWAPI::Position(0, 0);
				for (auto enemy : enemyUnits)
				{
					if (!enemy || !enemy->exists())
						continue;

					if (enemy->exists() && (*marine)->getDistance(enemy) < closestEnemy)
					{
						closestEnemy = (*marine)->getDistance(enemy);
						enemyGuy = enemy->getPosition();
					}
				}
				
				if ((!(*marine)->isAttacking() && !(*marine)->isMoving() && !(*marine)->isUnderAttack()) ||
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8))
				{
					if (!closeEnough((*marine)->getPosition(), dropTarget))
							(*marine)->attack(dropTarget);
				}
			}

			if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
			{
				if ((*marine)->isAttacking() && !(*marine)->isStimmed() &&
					((*marine)->getHitPoints() == (*marine)->getType().maxHitPoints()) &&
					(*marine)->getLastCommand().getType() != BWAPI::UnitCommandTypes::Use_Tech)
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
			if ((*medic)->isLoaded())
			{
				medic++;
				continue;
			}

			if (injured.size())
			{
				BWAPI::Unit closestInjured = injured.front();

				for (auto injuredSquadmate : injured)
				{
					if (!injuredSquadmate->isBeingHealed() && (*medic)->getDistance(injuredSquadmate) < (*medic)->getDistance(closestInjured))
					{
						closestInjured = injuredSquadmate;
					}
				}

				if (!closeEnough(closestInjured->getPosition(), (*medic)->getPosition()))
					(*medic)->attack(closestInjured);
			}
			else
			{
				if (_marines.size() && (*medic)->getDistance(_marines.front()) > 32)
				{
					(*medic)->attack(_marines.front()->getPosition());
				}
				else if (!closeEnough((*medic)->getPosition(), dropTarget))
					(*medic)->attack(dropTarget);
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
			if (tank->first->isLoaded())
			{
				tank++;
				continue;
			}

			if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode))
			{
				if (closeEnough(BWAPI::Position(dropTarget), tank->first->getPosition()) && !tank->first->isMoving() && !tank->first->isSieged())
				{
					if (!tank->first->isSieged())
						tank->first->siege();
					tank->second = BWAPI::Broodwar->getFrameCount();
				}
				else if (!closeEnough(BWAPI::Position(dropTarget), tank->first->getPosition()))
				{
					int targetDistance = 999999;
					BWAPI::Unit closestTargetForTank;
					for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
					{
						if (!enemy || !enemy->exists())
							continue;

						if (tank->first->getDistance(enemy) < targetDistance && !enemy->isFlying())
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
						tank->first->attack(BWAPI::Position(dropTarget));
					}
				}
			}
			else
			{
				if (!closeEnough(BWAPI::Position(dropTarget), tank->first->getPosition()))
					tank->first->attack(BWAPI::Position(dropTarget));
			}

			tank++;
		}
	}

	for (std::map<BWAPI::Unit, int>::iterator vulture = _vultures.begin(); vulture != _vultures.end();)
	{
		if (!vulture->first || !vulture->first->exists())
		{
			vulture = _vultures.erase(vulture);
		}
		else
		{
			if (vulture->first->isLoaded())
			{
				vulture++;
				continue;
			}

			if (!closeEnough(BWAPI::Position(dropTarget), vulture->first->getPosition()))
			{
				vulture->first->attack(BWAPI::Position(dropTarget));
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
			if ((*goliath)->isLoaded())
			{
				goliath++;
				continue;
			}

			if (!closeEnough(BWAPI::Position(dropTarget), (*goliath)->getPosition()))
			{
				(*goliath)->attack(BWAPI::Position(dropTarget));
			}

			goliath++;
		}
	}
}

void insanitybot::Squad::dropIdle()
{
	if (!dropSquadSize())
		return;

	if (_marines.size())
	{
		for (std::list <BWAPI::Unit>::iterator & marine = _marines.begin(); marine != _marines.end();)
		{
			if (!(*marine) || !(*marine)->exists())
			{
				marine = _marines.erase(marine);
			}
			else
			{
				marine++;
			}
		}
	}

	if (_medics.size())
	{
		for (std::list <BWAPI::Unit>::iterator & medic = _medics.begin(); medic != _medics.end();)
		{
			if (!(*medic) || !(*medic)->exists())
			{
				medic = _medics.erase(medic);
			}
			else
			{
				medic++;
			}
		}
	}
}

/***************************************************************
* Unit specific orders will be handled here
****************************************************************/
// Order the Marines around
void insanitybot::Squad::handleMarines(BWAPI::Position attackPoint, BWAPI::Position forwardGather, bool haveGathered, std::list<BWAPI::Unit>& injured, 
										std::list<BWAPI::Bullet> _activePsiStorms, std::list<BWAPI::Unit> _activeScarabs, BWAPI::Unitset enemyUnits, BWAPI::Unit target, BWAPI::Position gatherPoint)
{
	for (std::list <BWAPI::Unit>::iterator & marine = _marines.begin(); marine != _marines.end();)
	{
		if (!(*marine) || !(*marine)->exists())
		{
			marine = _marines.erase(marine);
		}
		else
		{
			bool dodging = false;

			if (_activePsiStorms.size())
			{
				for (auto storm : _activePsiStorms)
				{
					if ((*marine)->getDistance(storm->getPosition()) < 100)
					{
						(*marine)->move(stormDodge((*marine)->getPosition(), storm->getPosition()));
						dodging = true;
						break;
					}
				}
			}

			if (_activeScarabs.size())
			{
				for (auto scarab : _activeScarabs)
				{
					if (scarab->getOrderTarget() == (*marine))
					{
						if (!(*marine)->isStimmed() && BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
							(*marine)->useTech(BWAPI::TechTypes::Stim_Packs);
						else
							(*marine)->move(scarab->getPosition());

						dodging = true;
						break;
					}
					else if ((*marine)->getDistance(scarab->getOrderTargetPosition()) < 70)
					{
						(*marine)->move(scarabDodge((*marine)->getPosition(), scarab->getOrderTargetPosition()));
						dodging = true;
						break;
					}
				}
			}

			if (dodging)
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

			if (closestTankToTarget != NULL) // This marine is part of an All In squad
			{
				if (((*marine)->getDistance(closestTankToTarget) < 64 || isMaxSupply()) &&
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8 ||
					((*marine)->getLastCommand().getType() != BWAPI::UnitCommandTypes::Attack_Move &&
					(*marine)->getLastCommand().getType() != BWAPI::UnitCommandTypes::Attack_Unit)))
				{
					(*marine)->attack(attackPoint);
				}
				else if ((*marine)->getDistance(closestTankToTarget) > 128 &&
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8 ||
					((*marine)->getLastCommand().getType() != BWAPI::UnitCommandTypes::Attack_Move &&
					(*marine)->getLastCommand().getType() != BWAPI::UnitCommandTypes::Attack_Unit)))
				{
					(*marine)->attack(closestTankToTarget->getPosition());
				}
			}
			else if (target != NULL) // This Marine is part of a defensive squad that is clearing a nuetral structure
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
			}
			else if (gatherPoint != BWAPI::Position(0, 0)) // This Marine has been asked to gather up at a given point
			{
				if (!closeEnough(gatherPoint, (*marine)->getPosition()) &&
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8 ||
					((*marine)->getLastCommand().getType() != BWAPI::UnitCommandTypes::Attack_Move &&
					(*marine)->getLastCommand().getType() != BWAPI::UnitCommandTypes::Attack_Unit)))
						(*marine)->attack(gatherPoint);
			}
			else // This marine is part of a normal squad and has a position as a target
			{
				if ((!(*marine)->isAttacking() && !(*marine)->isMoving() && !(*marine)->isUnderAttack()) ||
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8 ||
					((*marine)->getLastCommand().getType() != BWAPI::UnitCommandTypes::Attack_Move &&
					(*marine)->getLastCommand().getType() != BWAPI::UnitCommandTypes::Attack_Unit)))
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

			if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
			{
				if ((*marine)->isAttacking() && !(*marine)->isStimmed() &&
					((*marine)->getHitPoints() == (*marine)->getType().maxHitPoints()) &&
					(*marine)->getLastCommand().getType() != BWAPI::UnitCommandTypes::Use_Tech)
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
}

// Firebats will currently only be used in SKTerran/Nuke builds
void insanitybot::Squad::handleFirebats(BWAPI::Position attackPoint, BWAPI::Position forwardGather, bool haveGathered, std::list<BWAPI::Unit>& injured,
	std::list<BWAPI::Bullet> _activePsiStorms, std::list<BWAPI::Unit> _activeScarabs, BWAPI::Unitset enemyUnits, BWAPI::Unit target, BWAPI::Position gatherPoint)
{
	for (std::list <BWAPI::Unit>::iterator & firebat = _firebats.begin(); firebat != _firebats.end();)
	{
		if (!(*firebat) || !(*firebat)->exists())
		{
			firebat = _firebats.erase(firebat);
		}
		else
		{
			bool dodging = false;

			if (_activePsiStorms.size())
			{
				for (auto storm : _activePsiStorms)
				{
					if ((*firebat)->getDistance(storm->getPosition()) < 100)
					{
						(*firebat)->move(stormDodge((*firebat)->getPosition(), storm->getPosition()));
						dodging = true;
						break;
					}
				}
			}

			if (_activeScarabs.size())
			{
				for (auto scarab : _activeScarabs)
				{
					if (scarab->getOrderTarget() == (*firebat))
					{
						if (!(*firebat)->isStimmed() && BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
							(*firebat)->useTech(BWAPI::TechTypes::Stim_Packs);
						else
							(*firebat)->move(scarab->getPosition());

						dodging = true;
						break;
					}
					else if ((*firebat)->getDistance(scarab->getOrderTargetPosition()) < 70)
					{
						(*firebat)->move(scarabDodge((*firebat)->getPosition(), scarab->getOrderTargetPosition()));
						dodging = true;
						break;
					}
				}
			}

			if (dodging)
			{
				firebat++;
				continue;
			}

			int closestEnemy = 9999999;
			BWAPI::Position enemyGuy = BWAPI::Position(0, 0);
			for (auto enemy : enemyUnits)
			{
				if (!enemy)
					continue;

				if (enemy->exists() && (*firebat)->getDistance(enemy) < closestEnemy)
				{
					closestEnemy = (*firebat)->getDistance(enemy);
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

			if (closestTankToTarget != NULL) // This firebat is part of an All In squad
			{
				if (((*firebat)->getDistance(closestTankToTarget) < 64 || isMaxSupply()) &&
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8))
				{
					(*firebat)->attack(attackPoint);
				}
				else if ((*firebat)->getDistance(closestTankToTarget) > 128 &&
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8))
				{
					(*firebat)->attack(closestTankToTarget->getPosition());
				}
			}
			else if (target != NULL) // This firebat is part of a defensive squad that is clearing a nuetral structure
			{
				if (!(*firebat)->isAttacking() && !(*firebat)->isMoving() && !(*firebat)->isUnderAttack())
				{
					if (target->exists())
						(*firebat)->attack(target);

					if (!closeEnough((*firebat)->getPosition(), target->getInitialPosition()))
					{
						(*firebat)->move(target->getInitialPosition());
					}
				}
			}
			else if (gatherPoint != BWAPI::Position(0, 0)) // This firebat has been asked to gather up at a given point
			{
				if (!closeEnough(gatherPoint, (*firebat)->getPosition()) &&
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8))
					(*firebat)->attack(gatherPoint);
			}
			else // This firebat is part of a normal squad and has a position as a target
			{
				if ((!(*firebat)->isAttacking() && !(*firebat)->isMoving() && !(*firebat)->isUnderAttack()) ||
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 8))
				{
					if (haveGathered)
					{
						if (!closeEnough((*firebat)->getPosition(), attackPoint))
							(*firebat)->attack(attackPoint);
					}
					else
					{
						if (!closeEnough((*firebat)->getPosition(), forwardGather))
						{
							(*firebat)->attack(forwardGather);
						}
					}
				}
			}

			if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
			{
				if ((*firebat)->isAttacking() && !(*firebat)->isStimmed() &&
					(*firebat)->getHitPoints() > 10 &&
					(*firebat)->getLastCommand().getType() != BWAPI::UnitCommandTypes::Use_Tech)
				{
					(*firebat)->useTech(BWAPI::TechTypes::Stim_Packs);
				}
			}

			if ((*firebat)->isCompleted() && (*firebat)->getHitPoints() < (*firebat)->getType().maxHitPoints())
			{
				injured.push_back((*firebat));
			}

			firebat++;
		}
	}
}

// Order the Medics around
void insanitybot::Squad::handleMedics(std::map<BWAPI::Unit, std::pair<BWAPI::Unit, int>>& _flareBD, std::list<BWAPI::Unit> injured, 
										std::list<BWAPI::Bullet> _activePsiStorms, std::list<BWAPI::Unit> _activeScarabs, BWAPI::Position gatherPoint)
{
	int spread = 0;

	for (std::list <BWAPI::Unit>::iterator medic = _medics.begin(); medic != _medics.end();)
	{
		if (!(*medic) || !(*medic)->exists())
		{
			medic = _medics.erase(medic);
		}
		else
		{
			bool dodging = false;
			if (_activePsiStorms.size())
			{
				for (auto storm : _activePsiStorms)
				{
					if ((*medic)->getDistance(storm->getPosition()) < 100)
					{
						(*medic)->move(stormDodge((*medic)->getPosition(), storm->getPosition()));
						dodging = true;
						break;
					}
				}
			}

			if (_activeScarabs.size())
			{
				for (auto scarab : _activeScarabs)
				{
					if (scarab->getOrderTarget() == (*medic))
					{
						(*medic)->move(scarab->getPosition());
						dodging = true;
						break;
					}
					else if ((*medic)->getDistance(scarab->getOrderTargetPosition()) < 70)
					{
						(*medic)->move(scarabDodge((*medic)->getPosition(), scarab->getOrderTargetPosition()));
						dodging = true;
						break;
					}
				}
			}

			if (dodging)
			{
				medic++;
				continue;
			}

			if (flareTarget((*medic), _flareBD))
			{

			}
			else if (injured.size())
			{
				BWAPI::Unit closestInjured = injured.front();

				for (auto injuredSquadmate : injured)
				{
					if (!injuredSquadmate->isBeingHealed() && (*medic)->getDistance(injuredSquadmate) < (*medic)->getDistance(closestInjured))
					{
						closestInjured = injuredSquadmate;
					}
				}

				if (!closeEnough(closestInjured->getPosition(), (*medic)->getPosition()))
					(*medic)->attack(closestInjured->getPosition());

			}
			else if(gatherPoint != BWAPI::Position(0,0))
			{
				if (!closeEnough(gatherPoint, (*medic)->getPosition()))
					(*medic)->move(gatherPoint);
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
					if ((*medic)->getDistance(*buddy) > 32)
						(*medic)->attack((*buddy)->getPosition());
				}
				else if (_marines.size())
				{
					if ((*medic)->getDistance(_marines.front()) > 32)
						(*medic)->attack(_marines.front()->getPosition());
				}
			}

			medic++;
		}
	}
}


void insanitybot::Squad::handleBCs(BWAPI::Position attackPoint, BWAPI::Position forwardGather, bool haveGathered, 
	BWAPI::Unitset enemyUnits, BWAPI::Unit target, BWAPI::Position gatherPoint)
{
	for (std::map<BWAPI::Unit, int>::iterator bc = _bcs.begin(); bc != _bcs.end();)
	{
		if (!bc->first || !bc->first->exists())
		{
			bc = _bcs.erase(bc);
		}
		else
		{
			int closestEnemy = 9999999;
			for (auto enemy : enemyUnits)
			{
				if (!enemy || !enemy->exists())
					continue;

				if (bc->first->getDistance(enemy) < closestEnemy)
				{
					closestEnemy = bc->first->getDistance(enemy);
				}
			}
			
			if (gatherPoint != BWAPI::Position(0, 0)) // This BC has been asked to gather up at a given point
			{
				if (!closeEnough(gatherPoint, bc->first->getPosition()) &&
					(closestEnemy > BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange() + 16))
					bc->first->attack(gatherPoint);
			}
			else // This BC has a position as a target
			{
				if ((!bc->first->isAttacking() && !bc->first->isMoving() && !bc->first->isUnderAttack()) ||
					(closestEnemy > BWAPI::UnitTypes::Terran_Battlecruiser.groundWeapon().maxRange() + 16))
				{
					if (haveGathered)
					{
						if (!closeEnough(bc->first->getPosition(), attackPoint))
							bc->first->attack(attackPoint);
					}
					else
					{
						if (!closeEnough(bc->first->getPosition(), forwardGather))
						{
							bc->first->attack(forwardGather);
						}
					}
				}
			}

			bc++;
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
	if ((nuker && nuker->exists()) || !BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Personnel_Cloaking))
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
	if (enemy == BWAPI::Position(0, 0))
		return;

	// Calculate the vector between the friendly and the enemy
	BWAPI::Position vector = BWAPI::Position(friendly->getPosition().x - enemy.x, enemy.y - friendly->getPosition().y);

	// Scale the vector by a factor of the friendly unit's max weapon range
	vector = vector * friendly->getType().groundWeapon().maxRange();

	BWAPI::Position kiteTo = friendly->getPosition() + vector;

	if (kiteTo.x < 0)
		kiteTo.x = 5;
	if (kiteTo.x > BWAPI::Broodwar->mapWidth())
		kiteTo.x = BWAPI::Broodwar->mapWidth() - 5;
	if (kiteTo.y < 0)
		kiteTo.y = 5;
	if (kiteTo.y > BWAPI::Broodwar->mapHeight())
		kiteTo.y = BWAPI::Broodwar->mapHeight() - 5;

	friendly->move(kiteTo);
}

// Storm dodging
BWAPI::Position insanitybot::Squad::stormDodge(BWAPI::Position friendly, BWAPI::Position stormPos)
{
	// Calculate the vector between the marine and the storm
	BWAPI::Position vector = BWAPI::Position(friendly.x - stormPos.x, stormPos.y - friendly.y);

	// Scale the vector by a factor of 48 (The size of the storm), and a small buffer, 16.
	vector = vector * (48 + 16);

	// Add the scaled vector to the marine's position to get the new position to move to
	BWAPI::Position dodgePosition = friendly + vector;

	// Make sure we don't order a unit outside of the map or we will crash
	if (dodgePosition.x < 0)
		dodgePosition.x = 5;
	if (dodgePosition.x > BWAPI::Broodwar->mapWidth())
		dodgePosition.x = BWAPI::Broodwar->mapWidth() - 5;
	if (dodgePosition.y < 0)
		dodgePosition.y = 5;
	if (dodgePosition.y > BWAPI::Broodwar->mapHeight())
		dodgePosition.y = BWAPI::Broodwar->mapHeight() - 5;

	return dodgePosition;
}

// Scarab dodging
BWAPI::Position insanitybot::Squad::scarabDodge(BWAPI::Position friendly, BWAPI::Position scarabTargetPos)
{
	// Calculate the distance between the marine and the target
	int distance = friendly.getDistance(scarabTargetPos);

	// Calculate the vector between the marine and the target
	BWAPI::Position vector = BWAPI::Position(friendly.x - scarabTargetPos.x, scarabTargetPos.y - friendly.y);

	// Normalize the vector (make it have a length of 1)
	vector = vector / distance;

	// Scale the vector by the splash radius plus a small buffer distance (e.g. 16)
	vector = vector * (40 + 16);

	// Add the scaled vector to the target position to get the new position to move to
	BWAPI::Position dodgePosition = scarabTargetPos + vector;

	// Make sure we don't order a unit outside of the map or we will crash
	if (dodgePosition.x < 0)
		dodgePosition.x = 5;
	if (dodgePosition.x > BWAPI::Broodwar->mapWidth())
		dodgePosition.x = BWAPI::Broodwar->mapWidth() - 5;
	if (dodgePosition.y < 0)
		dodgePosition.y = 5;
	if (dodgePosition.y > BWAPI::Broodwar->mapHeight())
		dodgePosition.y = BWAPI::Broodwar->mapHeight() - 5;

	return dodgePosition;
}

/***************************************************************
* Get active psy storms on the map
****************************************************************/
std::list<BWAPI::Bullet> insanitybot::Squad::getPsiStorms()
{
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

	return _activePsiStorms;
}

/***************************************************************
* Get active scarabs on the map
****************************************************************/
std::list<BWAPI::Unit> insanitybot::Squad::getScarabs()
{
	BWAPI::Unitset enemyUnits = BWAPI::Broodwar->enemy()->getUnits();

	// Scarab dodging attempt
	std::list<BWAPI::Unit> _activeScarabs;
	_activeScarabs.clear();

	for (auto unit : enemyUnits)
	{
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Scarab)
		{
			_activeScarabs.push_back(unit);
		}
	}

	return _activeScarabs;
}

/***************************************************************
* Vulture mine planting logic
****************************************************************/
bool insanitybot::Squad::canPlantMine(BWAPI::Unit vulture)
{
	if (!vulture->getSpiderMineCount())
		return false;

	BWAPI::Unitset nearbyMines = BWAPI::Broodwar->getUnitsInRadius(vulture->getPosition(), mineOffset, BWAPI::Filter::MaxHP == 20 && BWAPI::Filter::IsOwned);

	if (nearbyMines.size() > 0)
	{
		return false;
	}

	BWAPI::Unitset nearbyTanks = BWAPI::Broodwar->getUnitsInRadius(vulture->getPosition(), tankMineOffset, BWAPI::Filter::IsSieged && BWAPI::Filter::IsOwned);

	if (nearbyTanks.size() > 0)
	{
		return false;
	}

	return true;
}

bool insanitybot::Squad::shouldPlantMine(BWAPI::Unit vulture)
{
	BWAPI::Unitset nearbyFriendlyStructures = BWAPI::Broodwar->getUnitsInRadius(vulture->getPosition(), tankMineOffset, 
																				BWAPI::Filter::IsBuilding && BWAPI::Filter::IsOwned && !BWAPI::Filter::IsFlyingBuilding);

	if (rand() % 201 == 0 && vulture->isMoving() && vulture->getSpiderMineCount() > 0 && nearbyFriendlyStructures.size() == 0)
	{
		return true;
	}

	return false;
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

	bool limitTargets = (_tanks.size() == 0);

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
	int closestDistance = 800;

	for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (!enemy || !enemy->exists())
			continue;

		// Make sure it is a valid target
		if (!enemy->getType().isBuilding() && 
			((enemy->getType().isDetector() && limitTargets) || 
			(!limitTargets && !enemy->getType().isWorker())) &&
			!enemy->isIrradiated() && !enemy->isInvincible() && !enemy->isStasised() && 
			!enemy->isBlind() && enemy->isVisible() && notInFlareDB(enemy, _flareBD))
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


