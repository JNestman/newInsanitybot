#include "Squad.h"
#define _USE_MATH_DEFINES
#include <math.h>

namespace { auto & theMap = BWEM::Map::Instance(); }

using namespace insanitybot;

// Define KITE_DEBUG to draw each vulture's kiting state on screen.
//#define KITE_DEBUG

const int    TURNAROUND_FRAMES = 4;     // extra frames (on top of latency) to turn back before the weapon is ready
const int    RETREAT_COMMIT_FRAMES = 10;    // re-plan the retreat at least this often
const double RETREAT_HYSTERESIS = 64.0;  // px; extra threat radius while already retreating (prevents flapping)
const double MIN_RETREAT_STEP = 48.0;  // px
const double MAX_RETREAT_STEP = 160.0; // px
const double SPEED_FUTILITY_RATIO = 0.95;  // enemy at >= this fraction of our speed: don't bother kiting

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
	else if (unit->getType() == BWAPI::UnitTypes::Terran_Firebat)
	{
		_firebats.push_back(unit);
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

	_stableEnemyPos = BWAPI::Position(0, 0);
	_lastEnemyUpdateFrame = 0;
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
	handleTanks(attackPoint, forwardGather, haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, NULL, BWAPI::Position(0, 0));
	handleVultures(attackPoint, forwardGather, haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, NULL, BWAPI::Position(0, 0));
	handleGoliaths(attackPoint, forwardGather, haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, NULL, BWAPI::Position(0, 0));

	/************************************************************************************
	* Specialist Squad
	*************************************************************************************/
	handleGhosts(attackPoint, forwardGather, haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, NULL, BWAPI::Position(0, 0));

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
	handleTanks(BWAPI::Position(0, 0), BWAPI::Position(0, 0), haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, target, BWAPI::Position(0, 0));
	handleVultures(BWAPI::Position(0, 0), BWAPI::Position(0, 0), haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, target, BWAPI::Position(0, 0));
	handleGoliaths(BWAPI::Position(0, 0), BWAPI::Position(0, 0), haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, target, BWAPI::Position(0, 0));

	/****************************************************************************************
	* Specialists
	*****************************************************************************************/
	handleGhosts(BWAPI::Position(0, 0), BWAPI::Position(0, 0), haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, target, BWAPI::Position(0, 0));
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
	handleTanks(BWAPI::Position(0, 0), BWAPI::Position(0, 0), haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, NULL, gatherPoint);
	handleVultures(BWAPI::Position(0, 0), BWAPI::Position(0, 0), haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, NULL, gatherPoint);
	handleGoliaths(BWAPI::Position(0, 0), BWAPI::Position(0, 0), haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, NULL, gatherPoint);

	/****************************************************************************************
	* Specialists
	*****************************************************************************************/
	handleGhosts(BWAPI::Position(0, 0), BWAPI::Position(0, 0), haveGathered, injured, _activePsiStorms, _activeScarabs, enemyUnits, NULL, gatherPoint);

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

		if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine && unit->getDistance(BWAPI::Position(frontierLocation)) < 100)
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

	if (!dropship && !dropship->exists())
	{
		dropship = NULL;
	}
}

/***************************************************************
* Returns the tank in the squad closest to the current movement
* target (forwardGather before gathering, attackPoint after).
****************************************************************/
BWAPI::Unit insanitybot::Squad::getClosestTankToTarget(BWAPI::Position attackPoint, BWAPI::Position forwardGather, bool haveGathered)
{
	int closest = 999999;
	BWAPI::Unit closestTankToTarget = NULL;

	for (auto tank = _tanks.begin(); tank != _tanks.end();)
	{
		if (!tank->first || !tank->first->exists()) { tank = _tanks.erase(tank); continue; }

		BWAPI::Position ref = haveGathered ? attackPoint : forwardGather;
		int d = tank->first->getDistance(ref);
		if (d < closest) { closest = d; closestTankToTarget = tank->first; }

		++tank;
	}

	return closestTankToTarget;
}

// Arcing marines to look like we know what we're doing
std::vector<BWAPI::Position> insanitybot::Squad::getArcFormationPositions(
	BWAPI::Position tankPos,
	BWAPI::Position enemyPos,
	int marineCount,
	int baseRadius)
{
	std::vector<BWAPI::Position> slots;
	if (marineCount <= 0) return slots;

	BWAPI::Position center(
		(tankPos.x + enemyPos.x) / 2,
		(tankPos.y + enemyPos.y) / 2);

	double facingAngle = atan2(
		(double)(tankPos.y - enemyPos.y),
		(double)(tankPos.x - enemyPos.x));

	// Maximum marines per row before we start a new row behind the first.
	// At 28px spacing and 120 degree spread, comfortably fits ~8-10 per row
	// before the arc gets too wide.
	const int maxPerRow = 9;
	const double arcSpread = M_PI * (120.0 / 180.0); // fixed 120 degrees
	const double rowSpacing = 48.0; // pixels between rows, roughly 1.5 tiles

	int remaining = marineCount;
	int row = 0;

	while (remaining > 0)
	{
		int inThisRow = std::min(remaining, maxPerRow);
		double rowRadius = baseRadius + row * rowSpacing;

		for (int i = 0; i < inThisRow; ++i)
		{
			double t = (inThisRow == 1)
				? 0.0
				: -0.5 + (double)i / (double)(inThisRow - 1);

			double angle = facingAngle + t * arcSpread;

			BWAPI::Position candidate(
				center.x + (int)(rowRadius * cos(angle)),
				center.y + (int)(rowRadius * sin(angle)));

			// Terrain check: walk the slot back toward center until
			// it lands on walkable ground, stopping at the center itself.
			BWAPI::Position resolved = candidate;
			if (candidate.isValid())
			{
				bool walkable = BWAPI::Broodwar->isWalkable(
					BWAPI::WalkPosition(candidate));

				if (!walkable)
				{
					// Step inward along the radius in 8px increments
					const int stepSize = 8;
					const int maxSteps = (int)(rowRadius / stepSize);
					bool found = false;

					for (int step = 1; step <= maxSteps; ++step)
					{
						double inwardRatio = 1.0 - (step * stepSize) / rowRadius;
						BWAPI::Position nudged(
							center.x + (int)(rowRadius * inwardRatio * cos(angle)),
							center.y + (int)(rowRadius * inwardRatio * sin(angle)));

						if (nudged.isValid() && BWAPI::Broodwar->isWalkable(
							BWAPI::WalkPosition(nudged)))
						{
							resolved = nudged;
							found = true;
							break;
						}
					}

					// If nothing walkable found along the radius, fall back
					// to just in front of the tank
					if (!found)
					{
						resolved = BWAPI::Position(
							tankPos.x + (int)(32 * cos(facingAngle)),
							tankPos.y + (int)(32 * sin(facingAngle)));
					}
				}
			}
			else
			{
				// Slot fell off the map, pull it back to just in front of tank
				resolved = BWAPI::Position(
					tankPos.x + (int)(32 * cos(facingAngle)),
					tankPos.y + (int)(32 * sin(facingAngle)));
			}

			slots.push_back(resolved);
		}

		remaining -= inThisRow;
		++row;
	}

	return slots;
}

/***************************************************************
* Unit specific orders will be handled here
****************************************************************/
// Order the Marines around
void insanitybot::Squad::handleMarines(BWAPI::Position attackPoint, BWAPI::Position forwardGather, bool haveGathered, std::list<BWAPI::Unit>& injured,
	std::list<BWAPI::Bullet> _activePsiStorms, std::list<BWAPI::Unit> _activeScarabs, BWAPI::Unitset enemyUnits, BWAPI::Unit target, BWAPI::Position gatherPoint)
{
	// Pre-pass: find the closest enemy to the squad as a whole.
	int closestEnemyDist = 9999999;
	BWAPI::Position rawEnemyPos = BWAPI::Position(0, 0);

	BWAPI::Position reference = attackPoint;
	for (auto & m : _marines)
	{
		if (m && m->exists()) { reference = m->getPosition(); break; }
	}

	const int maxEngagementRange = 416;

	for (auto enemy : enemyUnits)
	{
		if (!enemy || !enemy->exists() ||
			enemy->getType() == BWAPI::UnitTypes::Zerg_Overlord ||
			enemy->getType() == BWAPI::UnitTypes::Protoss_Observer ||
			enemy->getType().isWorker()) continue;

		int d = reference.getApproxDistance(enemy->getPosition());
		if (d > maxEngagementRange) continue;
		if (d < closestEnemyDist) { closestEnemyDist = d; rawEnemyPos = enemy->getPosition(); }
	}

	const int positionUpdateThreshold = 64;
	const int frameUpdateCooldown = 12;
	int frameNow = BWAPI::Broodwar->getFrameCount();

	if (_stableEnemyPos == BWAPI::Position(0, 0) && rawEnemyPos != BWAPI::Position(0, 0))
	{
		_stableEnemyPos = rawEnemyPos;
		_lastEnemyUpdateFrame = frameNow;
	}
	else if (rawEnemyPos != BWAPI::Position(0, 0) &&
		frameNow - _lastEnemyUpdateFrame > frameUpdateCooldown &&
		_stableEnemyPos.getApproxDistance(rawEnemyPos) > positionUpdateThreshold)
	{
		_stableEnemyPos = rawEnemyPos;
		_lastEnemyUpdateFrame = frameNow;
	}

	BWAPI::Position enemyGuy = (_stableEnemyPos != BWAPI::Position(0, 0))
		? _stableEnemyPos : attackPoint;

	// Pre-pass: find the closest tank to the current movement target.
	int closest = 999999;
	BWAPI::Unit closestTankToTarget = NULL;
	for (auto tank = _tanks.begin(); tank != _tanks.end();)
	{
		if (!tank->first || !tank->first->exists()) { tank = _tanks.erase(tank); continue; }
		BWAPI::Position ref = haveGathered ? attackPoint : forwardGather;
		int d = tank->first->getDistance(ref);
		if (d < closest) { closest = d; closestTankToTarget = tank->first; }
		++tank;
	}

	// Pre-pass: compute arc slots if the trigger conditions are met.
	// Trigger: closestTankToTarget exists, is sieged, we have a known enemy
	// position, AND the bio is close enough to the tank to form a meaningful
	// shield. If the tank has raced ahead, suppress the arc and push marines
	// toward the tank instead until they close the gap.

	std::vector<BWAPI::Position> arcSlots;
	std::vector<bool> arcSlotClaimed;

	// Gap threshold: if the tank is more than this many pixels ahead of the
	// marine reference position, suppress the arc and push to close the gap.
	// 256px (~8 tiles) gives enough room for the tank to be slightly forward
	// without triggering a push, while catching the runaway tank case.
	const int maxTankGap = 256;

	bool tankTooFarAhead = closestTankToTarget != NULL &&
		reference.getApproxDistance(closestTankToTarget->getPosition()) > maxTankGap;

	bool useArcFormation = closestTankToTarget != NULL &&
		closestTankToTarget->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode &&
		enemyGuy != BWAPI::Position(0, 0) &&
		!tankTooFarAhead;

	if (useArcFormation)
	{
		arcSlots = getArcFormationPositions(
			closestTankToTarget->getPosition(),
			enemyGuy,
			(int)_marines.size(),
			96);
		arcSlotClaimed.assign(arcSlots.size(), false);
	}

	// Weapon range constant used across all branches
	const int marineRange = BWAPI::UnitTypes::Terran_Marine.groundWeapon().maxRange();

	// Main marine loop
	for (auto marine = _marines.begin(); marine != _marines.end();)
	{
		if (!(*marine) || !(*marine)->exists()) { marine = _marines.erase(marine); continue; }

		// --- Dodge checks (highest priority, interrupt everything) ---
		bool dodging = false;

		for (auto storm : _activePsiStorms)
		{
			if ((*marine)->getDistance(storm->getPosition()) < 100)
			{
				(*marine)->move(stormDodge((*marine)->getPosition(), storm->getPosition()));
				dodging = true;
				break;
			}
		}

		if (!dodging)
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

		if (dodging) { ++marine; continue; }

		// --- Unified engagement guard ---
		// True when an enemy is close enough that SC's own attack AI should
		// handle combat. We suppress movement commands in this state across
		// ALL branches so marines stop to shoot rather than running past.
		bool enemyInRange = closestEnemyDist <= marineRange + 16;

		// True when this marine's last command was already a move/attack to
		// the position we'd send it to � avoids reissuing identical orders.
		// Evaluated per-branch below since the target position varies.
		auto alreadyCommandedTo = [&](BWAPI::Position pos) -> bool
		{
			BWAPI::UnitCommand lastCmd = (*marine)->getLastCommand();
			return (lastCmd.getType() == BWAPI::UnitCommandTypes::Attack_Move ||
				lastCmd.getType() == BWAPI::UnitCommandTypes::Move) &&
				lastCmd.getTargetPosition() == pos;
		};

		// --- Branch: All-in squad (tank escort) ---
		if (closestTankToTarget != NULL)
		{
			if (isMaxSupply())
			{
				// Pure aggressive push � no formation.
				// Do nothing if enemy is in range, let SC handle the attack.
				if (!enemyInRange && !closeEnough((*marine)->getPosition(), attackPoint)
					&& !alreadyCommandedTo(attackPoint))
				{
					(*marine)->attack(attackPoint);
				}
			}
			else if (tankTooFarAhead)
			{
				// Tank has raced ahead of the bio � push marines directly toward
				// the tank to close the gap. Once within maxTankGap the arc
				// will activate on the next frame naturally.
				BWAPI::Position tankPos = closestTankToTarget->getPosition();
				bool alreadyHeadingToTank =
					(*marine)->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Move &&
					(*marine)->getLastCommand().getTargetPosition() == tankPos;

				if (!enemyInRange && !alreadyHeadingToTank &&
					!closeEnough((*marine)->getPosition(), tankPos))
				{
					(*marine)->attack(tankPos);
				}
			}
			else if (useArcFormation && !arcSlots.empty())
			{
				// Arc formation while tank is sieged.
				// Only move to slot if no enemy is in range.
				BWAPI::Position bestSlot = arcSlots.front();
				int bestDist = INT_MAX;
				int bestIdx = 0;
				for (int s = 0; s < (int)arcSlots.size(); ++s)
				{
					if (arcSlotClaimed[s] || !arcSlots[s].isValid()) continue;
					int d = (*marine)->getDistance(arcSlots[s]);
					if (d < bestDist) { bestDist = d; bestSlot = arcSlots[s]; bestIdx = s; }
				}
				arcSlotClaimed[bestIdx] = true;

				if (!enemyInRange &&
					(*marine)->getDistance(bestSlot) > 24 &&
					!alreadyCommandedTo(bestSlot))
				{
					(*marine)->attack(bestSlot);
				}
			}
			else
			{
				// Bounce behavior while tank is moving/unsieged.
				// Only reposition if no enemy is in range.
				if (!enemyInRange)
				{
					if ((*marine)->getDistance(closestTankToTarget) < 64
						&& !alreadyCommandedTo(attackPoint))
					{
						(*marine)->attack(attackPoint);
					}
					else if ((*marine)->getDistance(closestTankToTarget) > 128
						&& !alreadyCommandedTo(closestTankToTarget->getPosition()))
					{
						(*marine)->attack(closestTankToTarget->getPosition());
					}
				}
			}
		}

		// --- Branch: Clearing a neutral structure ---
		else if (target != NULL)
		{
			if (!target->exists()) { ++marine; continue; }

			// Use weapon range as the proximity check rather than closeEnough.
			// closeEnough (64px) is inside marine attack range (128px), meaning
			// marines would never reach the threshold and spam commands trying to.
			bool alreadyAttackingTarget =
				(*marine)->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
				(*marine)->getLastCommand().getTarget() == target;

			if (!alreadyAttackingTarget)
				(*marine)->attack(target);
		}

		// --- Branch: Gather point ---
		else if (gatherPoint != BWAPI::Position(0, 0))
		{
			// Only move to gather point if no enemy is threatening us
			// and we're not already heading there
			if (!enemyInRange &&
				!closeEnough(gatherPoint, (*marine)->getPosition()) &&
				!alreadyCommandedTo(gatherPoint))
			{
				(*marine)->attack(gatherPoint);
			}
		}

		// --- Branch: Normal squad ---
		else
		{
			// Only issue movement if no enemy is in range
			if (!enemyInRange)
			{
				BWAPI::Position dest = haveGathered ? attackPoint : forwardGather;
				if (!closeEnough((*marine)->getPosition(), dest) && !alreadyCommandedTo(dest))
					(*marine)->attack(dest);
			}
		}

		// --- Stim ---
		if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs))
		{
			bool alreadyStimmed = (*marine)->getStimTimer() > 0;
			bool stimCommandedThisFrame =
				(*marine)->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech &&
				(*marine)->getLastCommand().getTechType() == BWAPI::TechTypes::Stim_Packs;

			if ((*marine)->isAttacking() &&
				!alreadyStimmed &&
				!stimCommandedThisFrame &&
				(*marine)->getHitPoints() == (*marine)->getType().maxHitPoints())
			{
				(*marine)->useTech(BWAPI::TechTypes::Stim_Packs);
			}
		}

		if ((*marine)->isCompleted() && (*marine)->getHitPoints() < (*marine)->getType().maxHitPoints())
			injured.push_back(*marine);

		++marine;
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
				bool alreadyStimmed = (*firebat)->getStimTimer() > 0;
				bool stimCommandedThisFrame =
					(*firebat)->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech &&
					(*firebat)->getLastCommand().getTechType() == BWAPI::TechTypes::Stim_Packs;

				if ((*firebat)->isAttacking() &&
					!alreadyStimmed &&
					!stimCommandedThisFrame &&
					(*firebat)->getHitPoints() == (*firebat)->getType().maxHitPoints())
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

/***************************************************************
* Order the Siege Tanks around
****************************************************************/
void insanitybot::Squad::handleTanks(BWAPI::Position attackPoint, BWAPI::Position forwardGather, bool haveGathered, std::list<BWAPI::Unit>& injured,
	std::list<BWAPI::Bullet> _activePsiStorms, std::list<BWAPI::Unit> _activeScarabs, BWAPI::Unitset enemyUnits, BWAPI::Unit target, BWAPI::Position gatherPoint)
{
	for (auto tank = _tanks.begin(); tank != _tanks.end();)
	{
		if (!tank->first || !tank->first->exists()) { tank = _tanks.erase(tank); continue; }

		// --- Branch: Attacking a neutral structure ---
		if (target != NULL)
		{
			if (!tank->first->isAttacking() && !tank->first->isMoving() && !tank->first->isUnderAttack())
			{
				if (target->exists())
					tank->first->attack(target);
				if (!closeEnough(tank->first->getPosition(), target->getInitialPosition()))
					tank->first->move(target->getInitialPosition());
			}
			++tank;
			continue;
		}

		// --- Branch: Gather point ---
		if (gatherPoint != BWAPI::Position(0, 0))
		{
			if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode))
			{
				if (closeEnough(gatherPoint, tank->first->getPosition()) && !tank->first->isMoving() && !tank->first->isSieged())
				{
					tank->first->siege();
					tank->second = BWAPI::Broodwar->getFrameCount();
				}
				else if (!closeEnough(gatherPoint, tank->first->getPosition()))
				{
					int targetDistance = 999999;
					for (auto enemy : enemyUnits)
					{
						if (!enemy || !enemy->exists() || enemy->isFlying()) continue;
						int d = tank->first->getDistance(enemy);
						if (d < targetDistance) targetDistance = d;
					}

					if (targetDistance <= BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange() - 8)
					{
						if (!tank->first->isSieged())
							tank->first->siege();
						tank->second = BWAPI::Broodwar->getFrameCount();
					}

					if (tank->first->isSieged() && BWAPI::Broodwar->getFrameCount() - tank->second > 400)
						tank->first->unsiege();
					else if (!tank->first->isSieged())
						tank->first->attack(gatherPoint);
				}
			}
			else
			{
				if (!closeEnough(gatherPoint, tank->first->getPosition()))
					tank->first->attack(gatherPoint);
			}
			++tank;
			continue;
		}

		// --- Branch: Normal attack movement ---
		if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Tank_Siege_Mode))
		{
			BWAPI::Unit closestTankToTarget = getClosestTankToTarget(attackPoint, forwardGather, haveGathered);
			if (closestTankToTarget != NULL && tank->first != closestTankToTarget &&
				closestTankToTarget->isSieged() && tank->first->getDistance(closestTankToTarget) < 32)
			{
				if (!tank->first->isSieged())
					tank->first->siege();
				tank->second = BWAPI::Broodwar->getFrameCount();
				++tank;
				continue;
			}

			int targetDistance = 999999;
			for (auto enemy : enemyUnits)
			{
				if (!enemy || !enemy->exists() || enemy->isFlying()) continue;
				int d = tank->first->getDistance(enemy);
				if (d < targetDistance) targetDistance = d;
			}

			if (targetDistance <= BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().maxRange() - 8)
			{
				if (!tank->first->isSieged())
					tank->first->siege();
				tank->second = BWAPI::Broodwar->getFrameCount();
			}

			if (tank->first->isSieged() && BWAPI::Broodwar->getFrameCount() - tank->second > 400)
				tank->first->unsiege();
			else if (!tank->first->isSieged())
			{
				BWAPI::Position dest = haveGathered ? attackPoint : forwardGather;
				if (!closeEnough(tank->first->getPosition(), dest))
					tank->first->attack(dest);
			}
		}
		else
		{
			if (!tank->first->isAttacking() && !tank->first->isMoving() && !tank->first->isUnderAttack())
			{
				BWAPI::Position dest = haveGathered ? attackPoint : forwardGather;
				if (!closeEnough(tank->first->getPosition(), dest))
					tank->first->attack(dest);
			}
		}

		++tank;
	}
}

/***************************************************************
* Order the Vultures around
****************************************************************/
void insanitybot::Squad::handleVultures(BWAPI::Position attackPoint, BWAPI::Position forwardGather, bool haveGathered, std::list<BWAPI::Unit>& injured,
	std::list<BWAPI::Bullet> _activePsiStorms, std::list<BWAPI::Unit> _activeScarabs, BWAPI::Unitset enemyUnits, BWAPI::Unit target, BWAPI::Position gatherPoint)
{
	BWAPI::Unit closestTankToTarget = getClosestTankToTarget(attackPoint, forwardGather, haveGathered);

	auto tag = [&](BWAPI::Unit v, const char* s) {
		BWAPI::Broodwar->drawTextMap(v->getPosition() + BWAPI::Position(-20, -56), "HV:%s", s);
		};

	// Once, before the for loop (not once per vulture):
	for (auto it = kitingVultures.begin(); it != kitingVultures.end(); )
		it = (!*it || !(*it)->exists()) ? kitingVultures.erase(it) : std::next(it);

	const int latency = BWAPI::Broodwar->getLatencyFrames();

	for (auto vulture = _vultures.begin(); vulture != _vultures.end();)
	{
		if (!vulture->first || !vulture->first->exists()) { vulture = _vultures.erase(vulture); continue; }

		BWAPI::Unit u = vulture->first;
		const int range = u->getPlayer()->weaponMaxRange(u->getType().groundWeapon());

		// Hysteresis: easy to stay in kiting, harder to enter
		const bool wasKiting = kitingVultures.count(u) > 0;
		const int radius = range + (wasKiting ? 192 : 96);

		BWAPI::Unit closestEnemy = nullptr;
		int closestDist = INT_MAX;

		for (auto enemy : u->getUnitsInRadius(radius, BWAPI::Filter::IsEnemy))
		{
			if (!enemy || !enemy->exists() || !enemy->isVisible()) continue;

			const BWAPI::UnitType t = enemy->getType();
			if (t.isBuilding() || enemy->isFlying() || t.isInvincible()) continue;

			const int d = u->getDistance(enemy);
			if (d < closestDist)
			{
				closestDist = d;
				closestEnemy = enemy;
			}
		}

		// --- Branch: Attacking a neutral structure ---
		if (target != NULL)
		{
			if (!vulture->first->isAttacking() && !vulture->first->isMoving() && !vulture->first->isUnderAttack())
			{
				if (target->exists())
					vulture->first->attack(target);
				if (!closeEnough(vulture->first->getPosition(), target->getInitialPosition()))
					vulture->first->move(target->getInitialPosition());
			}
			tag(u, "neutral-target");   // in the target != NULL branch
			++vulture;
			continue;
		}

		if (closestEnemy)
		{
			vulture->second = 1;
			kitingVultures.insert(u);
			vultureKiteMicro(u, closestEnemy);
			++vulture;
			continue;
		}

		kitingVultures.erase(u); // nothing valid nearby: fall through to your normal logic
		tag(u, "no-enemy");

		// --- Branch: Normal attack movement ---
		// Mine planting state machine
		if (vulture->second > 1)
		{
			const int sinceCmd = BWAPI::Broodwar->getFrameCount() - vulture->second;
			const bool landed = sinceCmd > latency + 2;   // give the useTech command time to land
			if (landed && (u->getOrder() != BWAPI::Orders::PlaceMine || sinceCmd > 100))
				vulture->second = 1;
			else
			{
				tag(u, "mine-state");
				++vulture;
				continue;
			}
		}

		if (canPlantMine(vulture->first) && shouldPlantMine(vulture->first))
		{
			vulture->second = BWAPI::Broodwar->getFrameCount();
			vulture->first->useTech(BWAPI::TechTypes::Spider_Mines, vulture->first->getPosition());
			++vulture;
			continue;
		}

		// --- Branch: Gather point ---
		if (gatherPoint != BWAPI::Position(0, 0))
		{
			if (!closeEnough(gatherPoint, vulture->first->getPosition()) && vulture->first->getGroundWeaponCooldown() == 0)
				vulture->first->attack(gatherPoint);

			tag(u, "gather");
			++vulture;
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
			tag(u, "tank");
		}
		else
		{
			if (!vulture->first->isAttacking() && !vulture->first->isMoving() && !vulture->first->isUnderAttack()
				|| isAllInSquad)
			{
				BWAPI::Position dest = haveGathered ? attackPoint : forwardGather;
				if (!closeEnough(vulture->first->getPosition(), dest))
					vulture->first->attack(dest);
			}
			tag(u, "attack-move");
		}

		++vulture;
	}
}

/***************************************************************
* Order the Goliaths around
****************************************************************/
void insanitybot::Squad::handleGoliaths(BWAPI::Position attackPoint, BWAPI::Position forwardGather, bool haveGathered, std::list<BWAPI::Unit>& injured,
	std::list<BWAPI::Bullet> _activePsiStorms, std::list<BWAPI::Unit> _activeScarabs, BWAPI::Unitset enemyUnits, BWAPI::Unit target, BWAPI::Position gatherPoint)
{
	BWAPI::Unit closestTankToTarget = getClosestTankToTarget(attackPoint, forwardGather, haveGathered);

	// Pre-pass: check for floating building to chase (used in normal attack branch only)
	BWAPI::Unit floatingBuilding = NULL;
	for (auto enemyUnit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (!enemyUnit || !enemyUnit->exists()) continue;
		if (enemyUnit->getType().isBuilding() && enemyUnit->isFlying())
		{
			floatingBuilding = enemyUnit; break;
		}
	}

	for (auto goliath = _goliaths.begin(); goliath != _goliaths.end();)
	{
		if (!(*goliath) || !(*goliath)->exists()) { goliath = _goliaths.erase(goliath); continue; }

		// --- Branch: Attacking a neutral structure ---
		if (target != NULL)
		{
			if (!(*goliath)->isAttacking() && !(*goliath)->isMoving() && !(*goliath)->isUnderAttack())
			{
				if (target->exists())
					(*goliath)->attack(target);
				if (!closeEnough((*goliath)->getPosition(), target->getInitialPosition()))
					(*goliath)->move(target->getInitialPosition());
			}
			++goliath;
			continue;
		}

		// --- Branch: Gather point ---
		if (gatherPoint != BWAPI::Position(0, 0))
		{
			if (!closeEnough(gatherPoint, (*goliath)->getPosition()))
				(*goliath)->attack(gatherPoint);
			++goliath;
			continue;
		}

		// --- Branch: Normal attack movement ---
		if (floatingBuilding != NULL)
		{
			if (!closeEnough((*goliath)->getPosition(), floatingBuilding->getPosition()))
				(*goliath)->attack(floatingBuilding->getPosition());
		}
		else if (closestTankToTarget != NULL)
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
				BWAPI::Position dest = haveGathered ? attackPoint : forwardGather;
				if (!closeEnough((*goliath)->getPosition(), dest))
					(*goliath)->attack(dest);
			}
		}

		++goliath;
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
* Order the Ghosts around
****************************************************************/
void insanitybot::Squad::handleGhosts(BWAPI::Position attackPoint, BWAPI::Position forwardGather, bool haveGathered, std::list<BWAPI::Unit>& injured,
	std::list<BWAPI::Bullet> _activePsiStorms, std::list<BWAPI::Unit> _activeScarabs, BWAPI::Unitset enemyUnits, BWAPI::Unit target, BWAPI::Position gatherPoint)
{
	for (auto ghost = _ghosts.begin(); ghost != _ghosts.end();)
	{
		if (!(*ghost) || !(*ghost)->exists())
		{
			if ((*ghost) == nuker)
				nuker = NULL;

			ghost = _ghosts.erase(ghost);
			continue;
		}

		// Nuker is managed by unitManager � skip it here in all contexts
		if ((*ghost) == nuker) { ++ghost; continue; }

		// --- Branch: Attacking a neutral structure ---
		if (target != NULL)
		{
			if ((*ghost)->isCloaked())
				(*ghost)->decloak();
			else if (!(*ghost)->isAttacking() && !(*ghost)->isMoving() && !(*ghost)->isUnderAttack())
			{
				if (target->exists())
					(*ghost)->attack(target);
				if (!closeEnough((*ghost)->getPosition(), target->getInitialPosition()))
					(*ghost)->move(target->getInitialPosition());
			}
			++ghost;
			continue;
		}

		// --- Shared pre-pass: closest enemy, detection, and range (used by gather and normal attack branches) ---
		const int ghostRange = BWAPI::UnitTypes::Terran_Ghost.groundWeapon().maxRange();
		const int detectionScanRange = 400;

		int closestEnemy = 9999999;
		bool enemyDetectorNearby = false;

		for (auto enemy : enemyUnits)
		{
			if (!enemy || !enemy->exists()) continue;

			int d = (*ghost)->getDistance(enemy);
			if (d < closestEnemy) closestEnemy = d;

			if (!enemyDetectorNearby &&
				enemy->getType().isDetector() && !enemy->isBlind() &&
				d <= detectionScanRange)
			{
				enemyDetectorNearby = true;
			}
		}

		bool enemyInRange = closestEnemy <= ghostRange + 16;
		bool noEnemyAround = closestEnemy > ghostRange;

		auto alreadyCommandedTo = [&](BWAPI::Position pos) -> bool
		{
			BWAPI::UnitCommand lastCmd = (*ghost)->getLastCommand();
			return (lastCmd.getType() == BWAPI::UnitCommandTypes::Attack_Move ||
				lastCmd.getType() == BWAPI::UnitCommandTypes::Move) &&
				lastCmd.getTargetPosition() == pos;
		};

		// --- Shared cloak/decloak logic (gather and normal attack branches) ---
		auto handleCloaking = [&]() -> bool
		{
			if ((*ghost)->isCloaked())
			{
				if (enemyDetectorNearby || noEnemyAround)
					(*ghost)->decloak();
				return true;
			}
			else if (enemyInRange && !enemyDetectorNearby && (*ghost)->getEnergy() > 120)
			{
				(*ghost)->cloak();
				return true;
			}
			return false;
		};

		// --- Branch: Gather point ---
		if (gatherPoint != BWAPI::Position(0, 0))
		{
			if (!handleCloaking())
			{
				if (!closeEnough(gatherPoint, (*ghost)->getPosition()) &&
					!(*ghost)->isMoving() && !(*ghost)->isAttacking() && !(*ghost)->isAttackFrame())
					(*ghost)->attack(gatherPoint);
			}
			++ghost;
			continue;
		}

		// --- Branch: Normal attack movement ---
		if (!handleCloaking() && !enemyInRange)
		{
			BWAPI::Position dest = haveGathered ? attackPoint : forwardGather;
			if (!closeEnough((*ghost)->getPosition(), dest) && !alreadyCommandedTo(dest))
				(*ghost)->attack(dest);
		}

		++ghost;
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
// True if a straight line from `from` to `to` stays on walkable terrain.
// Samples every 16px and checks a small cross around each sample so the
// (fat) vulture doesn't clip cliff edges. NOTE: isWalkable() is terrain only;
// it ignores buildings and units.
bool lineIsWalkable(const BWAPI::Position& from, const BWAPI::Position& to)
{
	const double dx = to.x - from.x;
	const double dy = to.y - from.y;
	const int samples = std::max(1, (int)(std::sqrt(dx * dx + dy * dy) / 16.0));

	const int pad = 12;
	const int ox[] = { 0, pad, -pad, 0, 0 };
	const int oy[] = { 0, 0, 0, pad, -pad };

	for (int i = 1; i <= samples; ++i)
	{
		const int x = from.x + (int)(dx * i / samples);
		const int y = from.y + (int)(dy * i / samples);
		for (int k = 0; k < 5; ++k)
		{
			const BWAPI::Position p(x + ox[k], y + oy[k]);
			if (!p.isValid() || !BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(p)))
				return false;
		}
	}
	return true;
}

// Picks a walkable retreat point roughly opposite the threat. Tries straight
// away first, then fans out left/right so the vulture slides along walls
// instead of ramming them. Returns Positions::Invalid if cornered.
BWAPI::Position findRetreatPosition(BWAPI::Unit vulture, BWAPI::Unit target, double step)
{
	const BWAPI::Position vPos = vulture->getPosition();
	const BWAPI::Position tPos = target->getPosition();

	double dx = vPos.x - tPos.x;
	double dy = vPos.y - tPos.y;
	double len = std::sqrt(dx * dx + dy * dy);
	if (len < 1.0) { dx = 1.0; dy = 0.0; len = 1.0; }
	const double baseAngle = std::atan2(dy / len, dx / len);

	const double offsets[] = { 0.0, 0.35, -0.35, 0.7, -0.7, 1.05, -1.05, 1.4, -1.4 }; // radians

	BWAPI::Position best = BWAPI::Positions::Invalid;
	double bestScore = -1e9;

	for (double off : offsets)
	{
		const double a = baseAngle + off;
		const BWAPI::Position cand(vPos.x + (int)(std::cos(a) * step),
			vPos.y + (int)(std::sin(a) * step));

		if (!cand.isValid() || !lineIsWalkable(vPos, cand))
			continue;

		const double score = cand.getDistance(tPos) - std::abs(off) * 8.0;
		if (score > bestScore)
		{
			bestScore = score;
			best = cand;
		}
	}
	return best;
}

void insanitybot::Squad::vultureKiteMicro(BWAPI::Unit vulture, BWAPI::Unit target)
{
	auto tag = [&](const char* s) {
#ifdef KITE_DEBUG
		BWAPI::Broodwar->drawTextMap(vulture->getPosition() + BWAPI::Position(-20, -44), "%s", s);
#endif
		};
	if (!vulture || !target || !vulture->exists() || !target->exists()) return;
	if (target->isFlying() || !target->isVisible()) { tag("BAD-TARGET"); return; }
	//if (vulture->isStartingAttack() || vulture->isAttackFrame()) { tag("SHOT"); return; }

	// ---- 1. Gather real (upgrade-aware) numbers ------------------------------
	BWAPI::Player self = vulture->getPlayer();
	const BWAPI::WeaponType weapon = vulture->getType().groundWeapon();

	const int    distance = vulture->getDistance(target);
	const int    range = self->weaponMaxRange(weapon);
	const int    cooldown = vulture->getGroundWeaponCooldown();
	const double speed = self->topSpeed(vulture->getType()); // px/frame, includes Ion Thrusters
	const int    latency = BWAPI::Broodwar->getLatencyFrames();
	const int    frame = BWAPI::Broodwar->getFrameCount();

	const BWAPI::WeaponType enemyWeapon = target->getType().groundWeapon();
	const int    enemyRange = target->getPlayer()->weaponMaxRange(enemyWeapon);
	const double enemySpeed = target->getPlayer()->topSpeed(target->getType());

	const BWAPI::UnitCommand last = vulture->getLastCommand();

	auto debug = [&](const char* state)
		{
#ifdef KITE_DEBUG
			BWAPI::Broodwar->drawTextMap(vulture->getPosition() + BWAPI::Position(-20, -32),
				"%s cd=%d d=%d", state, cooldown, distance);
#endif
			(void)state;
		};

	if (cooldown == 0 && (vulture->isStartingAttack() || vulture->isAttackFrame()))
	{
		debug("SHOT");
		return;
	}

	auto issueAttack = [&]()
		{
			const bool sameOrder = last.getType() == BWAPI::UnitCommandTypes::Attack_Unit
				&& last.getTarget() == target;
			const bool settled = frame - vulture->getLastCommandFrame() > latency + 2;

			// Re-issue if it's a new order, or the old one silently dropped
			if (!sameOrder || (settled && vulture->isIdle()))
				vulture->attack(target);
		};

	// ---- 2. Cases where kiting is pointless ----------------------------------
	// Target can't shoot back, outranges/matches us, or is too fast to kite: just trade.
	if (enemyWeapon == BWAPI::WeaponTypes::None
		|| enemyRange >= range
		|| enemySpeed >= speed * SPEED_FUTILITY_RATIO)
	{
		debug("NO-KITE");
		issueAttack();
		return;
	}

	// ---- 3. Weapon ready (or about to be): ATTACK ----------------------------
	const int turnaround = latency + TURNAROUND_FRAMES;
	const int framesToWait = cooldown - turnaround;

	if (framesToWait <= 0)
	{
		debug("ATTACK");
		issueAttack();
		return;
	}

	// ---- 4. On cooldown ------------------------------------------------------
	const bool midRetreat = last.getType() == BWAPI::UnitCommandTypes::Move;

	// 4a. Keep a committed retreat alive, but only while it's demonstrably working.
	// If the order never landed, the unit stopped (blocked), it arrived, or the plan
	// is stale, fall through and re-plan instead of freezing.
	if (midRetreat)
	{
		const int  age = frame - vulture->getLastCommandFrame();
		const bool orderLanded = age <= latency + 2 || vulture->isMoving();
		const bool notArrived = vulture->getPosition().getDistance(last.getTargetPosition()) > 24.0;
		if (orderLanded && notArrived && age < RETREAT_COMMIT_FRAMES)
		{
			debug("RETREAT");
			return;
		}
	}

	// 4b. Threat test. Time-invariant: could the enemy reach us within one full
	// shot cycle? (Using the *remaining* cooldown here makes the test shrink as
	// fast as the enemy closes in, so the vulture never reacts.)
	const int    cycleFrames = weapon.damageCooldown() + turnaround;
	const double enemyReach = enemyRange + enemySpeed * cycleFrames
		+ (midRetreat ? RETREAT_HYSTERESIS : 0.0);

	if (distance > enemyReach)
	{
		debug("HOLD");
		issueAttack(); // closes to max range (and no further), then waits out the cooldown
		return;
	}

	// 4c. Retreat as far as the remaining cooldown allows; shorten the step if blocked.
	const double fullStep = std::max(MIN_RETREAT_STEP, std::min(MAX_RETREAT_STEP, speed * framesToWait));
	const double steps[] = { fullStep, fullStep * 0.6, MIN_RETREAT_STEP * 0.75 };

	BWAPI::Position retreat = BWAPI::Positions::Invalid;
	for (double s : steps)
	{
		retreat = findRetreatPosition(vulture, target, s);
		if (retreat.isValid()) break;
	}

	if (!retreat.isValid())
	{
		debug("CORNERED");
		issueAttack(); // nowhere to go: fight
		return;
	}

	debug("RETREAT");
	vulture->move(retreat);
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

	if (rand() % 100 == 0 && vulture->isMoving() && vulture->getSpiderMineCount() > 0 && nearbyFriendlyStructures.size() == 0)
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
	if (!medic || !medic->exists()) return false;
	if (medic->getEnergy() < 75 || !BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Optical_Flare))
		return false;

	bool limitTargets = (_tanks.size() == 0);

	// Check if this medic already has an assigned target
	auto it = _flareBD.find(medic);
	if (it != _flareBD.end())
	{
		BWAPI::Unit existingTarget = it->second.first;

		// Validate the existing target before acting on it.
		// If it's gone invisible, burrowed, or no longer exists/targetable,
		// drop it from the DB and fall through to find a new target.
		bool targetStillValid = existingTarget &&
			existingTarget->exists() &&
			existingTarget->isVisible() &&
			existingTarget->isDetected() &&
			!existingTarget->isBurrowed() &&
			!existingTarget->isCloaked() &&
			!existingTarget->isBlind() &&
			!existingTarget->isInvincible() &&
			!existingTarget->isStasised();

		if (targetStillValid)
		{
			medic->useTech(BWAPI::TechTypes::Optical_Flare, existingTarget);
			return true;
		}
		else
		{
			// Target is no longer valid � drop it and resume normal orders
			_flareBD.erase(it);
			return false;
		}
	}

	// Search for a new target
	BWAPI::Unit target = NULL;
	int closestDistance = 800;

	for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (!enemy || !enemy->exists()) continue;

		// Full validity check including burrow/cloak state
		bool validTarget = !enemy->getType().isBuilding() &&
			((limitTargets && enemy->getType().isDetector()) ||
			(!limitTargets && !enemy->getType().isWorker())) &&
			enemy->isVisible() &&
			enemy->isDetected() &&
			!enemy->isBurrowed() &&
			!enemy->isCloaked() &&
			!enemy->isBlind() &&
			!enemy->isIrradiated() &&
			!enemy->isInvincible() &&
			!enemy->isStasised() &&
			notInFlareDB(enemy, _flareBD) &&
			enemy->getType() != BWAPI::UnitTypes::Zerg_Zergling &&
			enemy->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			enemy->getType() != BWAPI::UnitTypes::Zerg_Larva;

		if (validTarget && medic->getDistance(enemy) < closestDistance)
		{
			target = enemy;
			closestDistance = medic->getDistance(enemy);
		}
	}

	if (target)
	{
		medic->useTech(BWAPI::TechTypes::Optical_Flare, target);
		_flareBD.insert({ medic, { target, BWAPI::Broodwar->getFrameCount() } });
	}

	return target != NULL;
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


