#include "BuildingPlacer.h"
#include "InformationManager.h"

using namespace insanitybot;

namespace { auto & theMap = BWEM::Map::Instance(); }

void BuildingPlacer::initialize()
{
	_reserveMap = std::vector< std::vector<bool> >(BWAPI::Broodwar->mapWidth(), std::vector<bool>(BWAPI::Broodwar->mapHeight(), false));

	reserveSpaceNearResources();
}

// Don't build in a position that blocks mining. Part of initialization.
void BuildingPlacer::reserveSpaceNearResources()
{
	for (const BWEM::Area & area : theMap.Areas())
	{
		for (const BWEM::Base & base : area.Bases())
		{
			// A tile close to the center of the resource depot building (which is 4x3 tiles).
			BWAPI::TilePosition baseTile = base.Location() + BWAPI::TilePosition(2, 1);

			// NOTE This still allows the bot to block mineral mining of some patches, but it's relatively rare.
			for (const auto mineral : base.Minerals())
			{
				BWAPI::TilePosition minTile = mineral->TopLeft();
				if (minTile.x < baseTile.x)
				{
					reserveTiles(minTile + BWAPI::TilePosition(2, 0), 1, 1);
				}
				else if (minTile.x > baseTile.x)
				{
					reserveTiles(minTile + BWAPI::TilePosition(-1, 0), 1, 1);
				}
				if (minTile.y < baseTile.y)
				{
					reserveTiles(minTile + BWAPI::TilePosition(0, 1), 2, 1);
				}
				else if (minTile.y > baseTile.y)
				{
					reserveTiles(minTile + BWAPI::TilePosition(0, -1), 2, 1);
				}
			}

			for (const auto gas : base.Geysers())
			{
				// Don't build on the right edge or a right corner of a geyser.
				// It causes workers to take a long path around. Other locations are OK.
				BWAPI::TilePosition gasTile = gas->TopLeft();
				//reserveTiles(gasTile, 4, 2);
				if (gasTile.x - baseTile.x > 2)
				{
					reserveTiles(gasTile + BWAPI::TilePosition(-1, 1), 3, 2);
				}
				else if (gasTile.x - baseTile.x < -2)
				{
					reserveTiles(gasTile + BWAPI::TilePosition(2, -1), 3, 2);
				}
				if (gasTile.y - baseTile.y > 2)
				{
					reserveTiles(gasTile + BWAPI::TilePosition(-1, -1), 2, 3);
				}
				else if (gasTile.y - baseTile.y < -2)
				{
					reserveTiles(gasTile + BWAPI::TilePosition(3, 0), 2, 3);
				}
			}
		}
	}
}

void BuildingPlacer::reserveTiles(BWAPI::TilePosition position, int width, int height)
{
	setReserve(position, width, height, true);
}

// Reserve or unreserve tiles.
// Tilepositions off the map are silently ignored; initialization code depends on it.
void BuildingPlacer::setReserve(BWAPI::TilePosition position, int width, int height, bool flag)
{
	int rwidth = _reserveMap.size();
	int rheight = _reserveMap[0].size();

	for (int x = std::max(position.x, 0); x < std::min(position.x + width, rwidth); ++x)
	{
		for (int y = std::max(position.y, 0); y < std::min(position.y + height, rheight); ++y)
		{
			_reserveMap[x][y] = flag;
		}
	}
}

// Can we build this building here with the specified amount of space around it?
bool BuildingPlacer::canBuildWithSpace(const BWAPI::TilePosition & position, const BWAPI::UnitType & b, int extraSpace) const
{
	// Can the building go here? This does not check the extra space or worry about future
	// buildings, but does all necessary checks for current obstructions of the building area itself.
	if (!BWAPI::Broodwar->canBuildHere(position, b))
	{
		return false;
	}

	// Is the entire area, including the extra space, free of obstructions
	// from possible future buildings?

	// Height and width of the building.
	int width(b.tileWidth());
	int height(b.tileHeight());

	// Allow space for terran addons. The space may be taller than necessary; it's easier that way.
	// All addons are 2x2 tiles.
	if (b.canBuildAddon())
	{
		width += 2;
	}

	// A rectangle covering the building spot.
	int x1 = position.x - extraSpace;
	int y1 = position.y - extraSpace;
	int x2 = position.x + width + extraSpace - 1;
	int y2 = position.y + height + extraSpace - 1;

	// The rectangle must fit on the map.
	if (x1 < 0 || x2 >= BWAPI::Broodwar->mapWidth() ||
		y1 < 0 || y2 >= BWAPI::Broodwar->mapHeight())
	{
		return false;
	}

	if (boxOverlapsBase(x1, y1, x2, y2))
	{
		return false;
	}

	// Every tile must be buildable and unreserved.
	for (int x = x1; x <= x2; ++x)
	{
		for (int y = y1; y <= y2; ++y)
		{
			if (!freeTile(x, y))
			{
				return false;
			}
		}
	}

	return true;
}

// The rectangle in tile coordinates overlaps with a resource depot location.
bool BuildingPlacer::boxOverlapsBase(int x1, int y1, int x2, int y2) const
{
	for (const BWEM::Area & area : theMap.Areas())
	{
		for (const BWEM::Base & base : area.Bases())
		{
			// The base location. It's the same size for all races.
			int bx1 = base.Location().x;
			int by1 = base.Location().y;
			int bx2 = bx1 + 3;
			int by2 = by1 + 2;

			if (!(x2 < bx1 || x1 > bx2 || y2 < by1 || y1 > by2))
			{
				return true;
			}
		}
	}

	// No base overlaps.
	return false;
}

bool BuildingPlacer::tileBlocksAddon(BWAPI::TilePosition position) const
{
	for (int i = 0; i <= 2; ++i)
	{
		for (BWAPI::Unit unit : BWAPI::Broodwar->getUnitsOnTile(position.x - i, position.y))
		{
			if (!unit)
				continue;

			if (unit->getType().canBuildAddon())
			{
				return true;
			}
		}
	}

	return false;
}

// The tile is free of permanent obstacles, including future ones from planned buildings.
// There might be a unit passing through, though.
// The caller must ensure that x and y are in range!
bool BuildingPlacer::freeTile(int x, int y) const
{
	//UAB_ASSERT(BWAPI::TilePosition(x, y).isValid(), "bad tile");

	if (!BWAPI::Broodwar->isBuildable(x, y, true) || _reserveMap[x][y])
	{
		return false;
	}
	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran && tileBlocksAddon(BWAPI::TilePosition(x, y)))
	{
		return false;
	}

	return true;
}

// Check that nothing obstructs the top of the building, including the corners.
// For example, if the building is o, then nothing must obstruct the tiles marked x:
//
//  x x x x
//    o o
//    o o
//
// Unlike canBuildHere(), the freeOn...() functions do not care if mobile units are on the tiles.
// They only care that the tiles are buildable (which implies walkable) and not reserved for future buildings.
bool BuildingPlacer::freeOnTop(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const
{
	int x1 = tile.x - 1;
	int x2 = tile.x + buildingType.tileWidth();
	int y = tile.y - 1;
	if (y < 0 || x1 < 0 || x2 >= BWAPI::Broodwar->mapWidth())
	{
		return false;
	}

	for (int x = x1; x <= x2; ++x)
	{
		if (!freeTile(x, y))
		{
			return false;
		}
	}
	return true;
}

//      x
//  o o x
//  o o x
//      x
bool BuildingPlacer::freeOnRight(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const
{
	int x = tile.x + buildingType.tileWidth();
	int y1 = tile.y - 1;
	int y2 = tile.y + buildingType.tileHeight();
	if (x >= BWAPI::Broodwar->mapWidth() || y1 < 0 || y2 >= BWAPI::Broodwar->mapHeight())
	{
		return false;
	}

	for (int y = y1; y <= y2; ++y)
	{
		if (!freeTile(x, y))
		{
			return false;
		}
	}
	return true;
}

//  x
//  x o o
//  x o o
//  x
bool BuildingPlacer::freeOnLeft(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const
{
	int x = tile.x - 1;
	int y1 = tile.y - 1;
	int y2 = tile.y + buildingType.tileHeight();
	if (x < 0 || y1 < 0 || y2 >= BWAPI::Broodwar->mapHeight())
	{
		return false;
	}

	for (int y = y1; y <= y2; ++y)
	{
		if (!freeTile(x, y))
		{
			return false;
		}
	}
	return true;
}

//    o o
//    o o
//  x x x x
bool BuildingPlacer::freeOnBottom(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const
{
	int x1 = tile.x - 1;
	int x2 = tile.x + buildingType.tileWidth();
	int y = tile.y + buildingType.tileHeight();
	if (y >= BWAPI::Broodwar->mapHeight() || x1 < 0 || x2 >= BWAPI::Broodwar->mapWidth())
	{
		return false;
	}

	for (int x = x1; x <= x2; ++x)
	{
		if (!freeTile(x, y))
		{
			return false;
		}
	}
	return true;
}

bool BuildingPlacer::freeOnAllSides(BWAPI::Unit building) const
{
	return
		freeOnTop(building->getTilePosition(), building->getType()) &&
		freeOnRight(building->getTilePosition(), building->getType()) &&
		freeOnLeft(building->getTilePosition(), building->getType()) &&
		freeOnBottom(building->getTilePosition(), building->getType());
}

BWAPI::TilePosition BuildingPlacer::getDesiredLocation(BWAPI::UnitType building, InformationManager & _infoManager, std::list<BWAPI::TilePosition> takenPositions)
{	
	// Our location we will build at
	BWAPI::TilePosition desiredLocation = BWAPI::TilePosition(1,1);

	// Check if it is an expansion
	if (building.isResourceDepot())
	{
		if (_infoManager.getPauseGas())
			_infoManager.setPauseGas(false);

		int shortest = 999999;
		int length = -1;
		for (auto base : _infoManager.getOtherBases())
		{
			if (!base.second)
			{
				BWAPI::Broodwar << "Null Base" << std::endl;
				continue;
			}

			if (_infoManager.getOwnedBases().size() < 2 && !base.second->baseHasGeyser()) continue;

			// Avoiding taking the center of the map if we can help it
			if (_infoManager.getOtherBases().size() > 1 &&
				(abs(BWAPI::Position(BWAPI::TilePosition(BWAPI::Broodwar->mapWidth() / 2, BWAPI::Broodwar->mapHeight() / 2)).x - base.first.x) <= 64 &&
					abs(BWAPI::Position(BWAPI::TilePosition(BWAPI::Broodwar->mapWidth() / 2, BWAPI::Broodwar->mapHeight() / 2)).y - base.first.y) <= 64)) continue;

			if (base.second->getRemainingMinerals() < 600) continue;

			theMap.GetPath(BWAPI::Position(base.first), BWAPI::Position(_infoManager.getMainPosition()), &length);
			
			if (length < 0) continue;

			if (length < shortest)
			{
				shortest = length;
				desiredLocation = base.second->Location();
			}
		}
		return desiredLocation;
	}
	//Bunkers go to choke positions
	else if (building == BWAPI::UnitTypes::Terran_Bunker)
	{
		if (_infoManager.isOneBasePlay(_infoManager.getStrategy()) && _infoManager.getStrategy() != "MechAllIn" &&
			!_infoManager.isExpanding())
		{
			bool bunkerSpotBuildable = BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(_infoManager.getMainBunkerPos()), true) &&
				BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(_infoManager.getMainBunkerPos()) + BWAPI::TilePosition(1, 0), true) &&
				BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(_infoManager.getMainBunkerPos()) + BWAPI::TilePosition(0, 1), true) &&
				BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(_infoManager.getMainBunkerPos()) + BWAPI::TilePosition(1, 1), true);

			if (bunkerSpotBuildable)
				desiredLocation = _infoManager.getMainBunkerPos();
			else
				desiredLocation = getPositionNear(BWAPI::UnitTypes::Terran_Bunker, _infoManager.getMainBunkerPos(), _infoManager.isMech(_infoManager.getStrategy()));
		}
		else
		{
			// For some reason, this behavior leads to better bunker placement in the natural.
			bool bunkerSpotBuildable = BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(_infoManager.getMainBunkerPos()), true); /*&&
				BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(_infoManager.getMainBunkerPos()) + BWAPI::TilePosition(1, 0)) &&
				BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(_infoManager.getMainBunkerPos()) + BWAPI::TilePosition(0, 1)) &&
				BWAPI::Broodwar->isBuildable(BWAPI::TilePosition(_infoManager.getMainBunkerPos()) + BWAPI::TilePosition(1, 1));*/

			if (bunkerSpotBuildable)
				desiredLocation = _infoManager.getNatBunkerPos();
			else
				desiredLocation = getPositionNear(BWAPI::UnitTypes::Terran_Bunker, _infoManager.getNatBunkerPos(), _infoManager.isMech(_infoManager.getStrategy()));
				//desiredLocation = BWAPI::Broodwar->getBuildLocation(BWAPI::UnitTypes::Terran_Bunker, _infoManager.getNatBunkerPos());
		}

		return desiredLocation;
	}
	// Refineries should be placed at taken expansions
	else if (building.isRefinery())
	{
		for (auto base : _infoManager.getOwnedBases())
		{
			if (base.second->baseHasGeyser() && 
				(base.second->getBaseCommandCenter() && base.second->getBaseCommandCenter()->exists()) &&
				!base.second->getBaseCommandCenter()->isBeingConstructed())
			{
				// ToDo: This check doesn't catch dead refineries that haven't been caught elsewhere
				if (!base.second->baseHasRefinery() || base.second->getBaseRefinery()->getType() != BWAPI::UnitTypes::Terran_Refinery)
				{
					desiredLocation = base.second->Geysers().front()->TopLeft();
				}
			}
		}
		return desiredLocation;
	}

	// Loop through our units and place buildings of the same size next to each other
	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (!unit)
			continue;

		if (unit->isLifted())
			continue;

		if (unit->getType().isRefinery() || unit->getType() == BWAPI::UnitTypes::Terran_Bunker 
			|| !unit->getType().isBuilding()) continue;

		BWAPI::TilePosition tile;

		// Barracks can be grouped much closer together
		if ((unit->getType() == BWAPI::UnitTypes::Terran_Barracks && building == BWAPI::UnitTypes::Terran_Barracks) ||
			(building.tileSize() == BWAPI::UnitTypes::Terran_Supply_Depot.tileSize() && unit->getType().tileSize() == BWAPI::UnitTypes::Terran_Supply_Depot.tileSize()))
		{
			// Left.
			tile = unit->getTilePosition() + BWAPI::TilePosition(-building.tileWidth(), 0);
			bool taken = false;
			for (auto tileTaken : takenPositions)
			{
				if (tileTaken == tile)
					taken = true;
			}
			if (canBuildWithSpace(tile, building, 0) &&
				freeOnLeft(tile, building) &&
				!taken)
			{
				return tile;
			}

			// Right.
			tile = unit->getTilePosition() + BWAPI::TilePosition(building.tileWidth(), 0);
			taken = false;
			for (auto tileTaken : takenPositions)
			{
				if (tileTaken == tile)
					taken = true;
			}
			if (canBuildWithSpace(tile, building, 0) &&
				freeOnRight(tile, building) &&
				!taken)
			{
				return tile;
			}

			// Above.
			tile = unit->getTilePosition() + BWAPI::TilePosition(0, -building.tileHeight());
			taken = false;
			for (auto tileTaken : takenPositions)
			{
				if (tileTaken == tile)
					taken = true;
			}
			if (canBuildWithSpace(tile, building, 0) &&
				freeOnTop(tile, building) &&
				!taken)
			{
				return tile;
			}

			// Below.
			tile = unit->getTilePosition() + BWAPI::TilePosition(0, unit->getType().tileHeight());
			taken = false;
			for (auto tileTaken : takenPositions)
			{
				if (tileTaken == tile)
					taken = true;
			}
			if (canBuildWithSpace(tile, building, 0) &&
				freeOnBottom(tile, building) &&
				!taken)
			{
				return tile;
			}
		}
		// Stack factories
		else if (unit->getType() == BWAPI::UnitTypes::Terran_Factory && building == BWAPI::UnitTypes::Terran_Factory)
		{
			// Above.
			tile = unit->getTilePosition() + BWAPI::TilePosition(0, -building.tileHeight());
			bool taken = false;
			for (auto tileTaken : takenPositions)
			{
				if (tileTaken == tile)
					taken = true;
			}
			if (canBuildWithSpace(tile, building, 0) &&
				freeOnTop(tile, building) &&
				!taken)
			{
				return tile;
			}

			// Below.
			tile = unit->getTilePosition() + BWAPI::TilePosition(0, unit->getType().tileHeight());
			taken = false;
			for (auto tileTaken : takenPositions)
			{
				if (tileTaken == tile)
					taken = true;
			}
			if (canBuildWithSpace(tile, building, 0) &&
				freeOnBottom(tile, building) &&
				!taken)
			{
				return tile;
			}

		}
		else
		{
			if (unit->getType().tileWidth() == building.tileWidth())
			{
				// Above.
				tile = unit->getTilePosition() + BWAPI::TilePosition(0, -building.tileHeight());
				bool taken = false;
				for (auto tileTaken : takenPositions)
				{
					if (tileTaken == tile)
						taken = true;
				}
				if (canBuildWithSpace(tile, building, 0) &&
					freeOnTop(tile, building) &&
					freeOnLeft(tile, building) &&
					freeOnRight(tile, building) &&
					!taken)
				{
					return tile;
				}

				// Below.
				tile = unit->getTilePosition() + BWAPI::TilePosition(0, unit->getType().tileHeight());
				taken = false;
				for (auto tileTaken : takenPositions)
				{
					if (tileTaken == tile)
						taken = true;
				}
				if (canBuildWithSpace(tile, building, 0) &&
					freeOnBottom(tile, building) &&
					freeOnLeft(tile, building) &&
					freeOnRight(tile, building) &&
					!taken)
				{
					return tile;
				}
			}

			// Buildings that may have addons in the future should be grouped vertically if at all.
			if (unit->getType().tileHeight() == building.tileHeight() &&
				!building.canBuildAddon() &&
				!unit->getType().canBuildAddon())
			{
				// Left.
				tile = unit->getTilePosition() + BWAPI::TilePosition(-building.tileWidth(), 0);
				bool taken = false;
				for (auto tileTaken : takenPositions)
				{
					if (tileTaken == tile)
						taken = true;
				}
				if (canBuildWithSpace(tile, building, 0) &&
					freeOnTop(tile, building) &&
					freeOnLeft(tile, building) &&
					freeOnBottom(tile, building) &&
					!taken)
				{
					return tile;
				}

				// Right.
				tile = unit->getTilePosition() + BWAPI::TilePosition(building.tileWidth(), 0);
				taken = false;
				for (auto tileTaken : takenPositions)
				{
					if (tileTaken == tile)
						taken = true;
				}
				if (canBuildWithSpace(tile, building, 0) &&
					freeOnBottom(tile, building) &&
					freeOnLeft(tile, building) &&
					freeOnTop(tile, building) &&
					!taken)
				{
					return tile;
				}
			}
		}
	}

	if (building.tileSize() == BWAPI::UnitTypes::Terran_Barracks.tileSize())
	{
		BWAPI::Position halfwayPoint = BWAPI::Position((BWAPI::Position(_infoManager.getMainPosition()).x + _infoManager.getMainChokePos().x) / 2, (BWAPI::Position(_infoManager.getMainPosition()).y + _infoManager.getMainChokePos().y) / 2);
		desiredLocation = BWAPI::TilePosition(halfwayPoint);
	}
	else if (building.tileSize() == BWAPI::UnitTypes::Terran_Supply_Depot.tileSize())
	{
		// Top of the map
		if (_infoManager.getMainPosition().y < 30)
		{
			BWAPI::Position edge = BWAPI::Position((BWAPI::Position(_infoManager.getMainPosition()).x), 0);

			if (BWAPI::Broodwar->mapHash() == "de2ada75fbc741cfa261ee467bf6416b10f9e301") // Python
			{
				edge += BWAPI::Position(100, 0);
			}

			desiredLocation = BWAPI::TilePosition(edge);
		}
		// Bottom of Map
		else if (BWAPI::Broodwar->mapHeight() - _infoManager.getMainPosition().y < 30)
		{
			BWAPI::TilePosition temp = BWAPI::TilePosition(BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
			BWAPI::Position edge = BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, BWAPI::Position(temp).y);

			if (BWAPI::Broodwar->mapHash() == "de2ada75fbc741cfa261ee467bf6416b10f9e301") // Python
			{
				edge += BWAPI::Position(-300, 0);
			}

			desiredLocation = BWAPI::TilePosition(edge);
		}
		// Left side of map
		else if (_infoManager.getMainPosition().x < 30)
		{
			BWAPI::Position edge = BWAPI::Position(0, (BWAPI::Position(_infoManager.getMainPosition()).y));

			if (BWAPI::Broodwar->mapHash() == "de2ada75fbc741cfa261ee467bf6416b10f9e301") // Python
			{
				edge += BWAPI::Position(0, 100);
			}

			desiredLocation = BWAPI::TilePosition(edge);
		}
		// Right side of map
		else if (BWAPI::Broodwar->mapWidth() - _infoManager.getMainPosition().x < 30)
		{
			BWAPI::TilePosition temp = BWAPI::TilePosition(BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
			BWAPI::Position edge = BWAPI::Position(BWAPI::Position(temp).x, BWAPI::Position(_infoManager.getMainPosition()).y);

			if (BWAPI::Broodwar->mapHash() == "de2ada75fbc741cfa261ee467bf6416b10f9e301") // Python
			{
				edge += BWAPI::Position(0, -100);
			}

			desiredLocation = BWAPI::TilePosition(edge);
		}
		else
		{
			desiredLocation = _infoManager.getMainPosition();
		}
	}
	// If it is not, grab a standard location
	else
	{
		desiredLocation = _infoManager.getMainPosition();
	}

	desiredLocation = getPositionNear(building, desiredLocation, _infoManager.isMech(_infoManager.getStrategy()));

	return desiredLocation;
}

BWAPI::TilePosition BuildingPlacer::getSupplyLocation(BWAPI::UnitType building, InformationManager & _infoManager)
{
	// Our location we will build at
	BWAPI::TilePosition desiredLocation = BWAPI::Broodwar->getBuildLocation(building, _infoManager.getMainPosition());
	bool top = false;
	bool bottom = false;
	bool left = false;
	bool right = false;
	
	// Top of the map
	if (_infoManager.getMainPosition().y < 30)
	{
		BWAPI::Position edge = BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, 0);

		if (BWAPI::Broodwar->mapHash() == "de2ada75fbc741cfa261ee467bf6416b10f9e301") // Python
		{
			edge += BWAPI::Position(100, 0);
		}
		else if (BWAPI::Broodwar->mapHash() == "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b") //Destination
		{
			edge += BWAPI::Position(-200, 0);
		}
		else if (BWAPI::Broodwar->mapHash() == "a220d93efdf05a439b83546a579953c63c863ca7") // Empire of the Sun
		{
			if (_infoManager.getMainPosition().x < 30) // Top left
			{
				edge += BWAPI::Position(-BWAPI::Position(_infoManager.getMainPosition()).x, 600);
			}
			else // Top right
			{
				edge += BWAPI::Position(100, 600);
			}
		}
		else if (BWAPI::Broodwar->mapHash() == "a220d93efdf05a439b83546a579953c63c863ca7") // Empire of the Sun
		{
			if (_infoManager.getMainPosition().x < 30) // Top left
			{
				edge += BWAPI::Position(-BWAPI::Position(_infoManager.getMainPosition()).x, 200);
			}
			else // Top right
			{
				edge += BWAPI::Position(100, 200);
			}
		}
		else if (BWAPI::Broodwar->mapHash() == "d2f5633cc4bb0fca13cd1250729d5530c82c7451") // Fighting Spirit
		{
			if (_infoManager.getMainPosition().x > 30) // Top Right
			{
				edge += BWAPI::Position(100, 400);
			}
			
		}

		desiredLocation = BWAPI::Broodwar->getBuildLocation(building, BWAPI::TilePosition(edge));
		top = true;
	}
	// Bottom of Map
	else if (BWAPI::Broodwar->mapHeight() - _infoManager.getMainPosition().y < 30)
	{
		BWAPI::Position edge = BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x, BWAPI::Position(_infoManager.getMainPosition()).y + 200);

		if (BWAPI::Broodwar->mapHash() == "de2ada75fbc741cfa261ee467bf6416b10f9e301") // Python
		{
			edge += BWAPI::Position(-300, 0);
		}
		else if (BWAPI::Broodwar->mapHash() == "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b") //Destination
		{
			edge += BWAPI::Position(200, 0);
		}
		else if (BWAPI::Broodwar->mapHash() == "a220d93efdf05a439b83546a579953c63c863ca7") // Empire of the Sun
		{
			if (_infoManager.getMainPosition().x < 30) // Bottom left
			{
				edge += BWAPI::Position(-BWAPI::Position(_infoManager.getMainPosition()).x, -400);
			}
			else // Bottom right
			{
				edge += BWAPI::Position(100, -400);
			}
		}
		else if (BWAPI::Broodwar->mapHash() == "d2f5633cc4bb0fca13cd1250729d5530c82c7451") // Fighting Spirit
		{
			if (_infoManager.getMainPosition().x > 30) // Bottom Right
			{
				edge += BWAPI::Position(-200, 0);
			}
			else // Bottom Left
			{
				edge += BWAPI::Position(-100, -400);
			}

		}
		else if (BWAPI::Broodwar->mapHash() == "df21ac8f19f805e1e0d4e9aa9484969528195d9f") // Jade
		{
			if (_infoManager.getMainPosition().x > 30) // Bottom Right
			{
				edge += BWAPI::Position(0, -400);
			}
		}

		desiredLocation = BWAPI::Broodwar->getBuildLocation(building, BWAPI::TilePosition(edge));
		bottom = true;
	}
	// Left side of map
	else if (_infoManager.getMainPosition().x < 30)
	{
		BWAPI::Position edge = BWAPI::Position(0, (BWAPI::Position(_infoManager.getMainPosition()).y));

		if (BWAPI::Broodwar->mapHash() == "de2ada75fbc741cfa261ee467bf6416b10f9e301") // Python
		{
			edge += BWAPI::Position(0, 100);
		}
		else if (BWAPI::Broodwar->mapHash() == "6f8da3c3cc8d08d9cf882700efa049280aedca8c") // Heartbreak Ridge
		{
			edge += BWAPI::Position(0, -200);
		}
		else if (BWAPI::Broodwar->mapHash() == "c8386b87051f6773f6b2681b0e8318244aa086a6") // Neo Moon Glaive
		{
			edge += BWAPI::Position(0, -200);
		}

		desiredLocation = BWAPI::Broodwar->getBuildLocation(building, BWAPI::TilePosition(edge));
		left = true;
	}
	// Right side of map
	else if (BWAPI::Broodwar->mapWidth() - _infoManager.getMainPosition().x < 30)
	{
		BWAPI::Position edge = BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x + 60, BWAPI::Position(_infoManager.getMainPosition()).y);

		if (BWAPI::Broodwar->mapHash() == "de2ada75fbc741cfa261ee467bf6416b10f9e301") // Python
		{
			edge += BWAPI::Position(0, -100);
		}
		else if (BWAPI::Broodwar->mapHash() == "6f8da3c3cc8d08d9cf882700efa049280aedca8c") // Heartbreak Ridge
		{
			edge += BWAPI::Position(0, 200);
		}
		else if (BWAPI::Broodwar->mapHash() == "c8386b87051f6773f6b2681b0e8318244aa086a6") // Neo Moon Glaive
		{
			edge += BWAPI::Position(0, 200);
		}
		else if (BWAPI::Broodwar->mapHash() == "0409ca0d7fe0c7f4083a70996a8f28f664d2fe37") // Icarus
		{
			edge += BWAPI::Position(0, -200);
		}

		desiredLocation = BWAPI::Broodwar->getBuildLocation(building, BWAPI::TilePosition(edge));
		right = true;
	}


	// Loop through our units and place buildings of the same size next to each other
	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (!unit)
			continue;

		if (unit->getType().isRefinery() || unit->getType() == BWAPI::UnitTypes::Terran_Bunker
			|| !unit->getType().isBuilding()) continue;

		BWAPI::TilePosition tile;

		if (unit->getType().tileWidth() == building.tileWidth())
		{
			// Left.
			tile = unit->getTilePosition() + BWAPI::TilePosition(-building.tileWidth(), 0);
			if (canBuildWithSpace(tile, building, 0) &&
				freeOnLeft(tile, building))
			{
				return tile;
			}
			/*if (right)
			{
				if (canBuildWithSpace(tile, building, 0) &&
					freeOnLeft(tile, building))
				{
					return tile;
				}
			}
			else
			{
				if (canBuildWithSpace(tile, building, 0) &&
					freeOnTop(tile, building) &&
					freeOnLeft(tile, building) &&
					freeOnBottom(tile, building))
				{
					return tile;
				}
			}*/

			// Right.
			tile = unit->getTilePosition() + BWAPI::TilePosition(building.tileWidth(), 0);
			if (canBuildWithSpace(tile, building, 0) &&
				freeOnRight(tile, building))
			{
				return tile;
			}
			/*if (left)
			{
				if (canBuildWithSpace(tile, building, 0) &&
					freeOnRight(tile, building))
				{
					return tile;
				}
			}
			else
			{
				if (canBuildWithSpace(tile, building, 0) &&
					freeOnBottom(tile, building) &&
					freeOnRight(tile, building) &&
					freeOnTop(tile, building))
				{
					return tile;
				}
			}*/

			// Above.
			tile = unit->getTilePosition() + BWAPI::TilePosition(0, -building.tileHeight());
			if (canBuildWithSpace(tile, building, 0) &&
				freeOnTop(tile, building))
			{
				return tile;
			}
			/*if (bottom)
			{
				if (canBuildWithSpace(tile, building, 0) &&
					freeOnTop(tile, building))
				{
					return tile;
				}
			}
			else
			{
				if (canBuildWithSpace(tile, building, 0) &&
					freeOnTop(tile, building) &&
					freeOnLeft(tile, building) &&
					freeOnRight(tile, building))
				{
					return tile;
				}
			}*/

			// Below.
			tile = unit->getTilePosition() + BWAPI::TilePosition(0, unit->getType().tileHeight());
			if (canBuildWithSpace(tile, building, 0) &&
				freeOnBottom(tile, building))
			{
				return tile;
			}
			/*if (top)
			{
				if (canBuildWithSpace(tile, building, 0) &&
					freeOnBottom(tile, building))
				{
					return tile;
				}
			}
			else
			{
				if (canBuildWithSpace(tile, building, 0) &&
					freeOnBottom(tile, building) &&
					freeOnLeft(tile, building) &&
					freeOnRight(tile, building))
				{
					return tile;
				}
			}*/
		}
	}

	return desiredLocation;
}

BWAPI::TilePosition insanitybot::BuildingPlacer::getTurretLocation(InformationManager & _infoManager)
{
	if (_infoManager.enemyHasDtLurker() && _infoManager.getBunkers().size())
	{
		for (auto bunker : _infoManager.getBunkers())
		{
			bool bunkerNeedsTurret = true;

			for (auto turret : _infoManager.getTurrets())
			{
				if (bunker->getDistance(turret->getPosition()) < 128)
				{
					bunkerNeedsTurret = false;
					break;
				}
			}

			if (bunkerNeedsTurret)
				 return getPositionNear(BWAPI::UnitTypes::Terran_Missile_Turret, bunker->getTilePosition(), _infoManager.isMech(_infoManager.getStrategy()));
		}
	}

	for (auto & base : _infoManager.getOwnedBases())
	{
		if (base.second->numTurretsHere() < 3)
		{
			BWAPI::TilePosition commandCenter = base.second->Location();

			//top right
			if (validTurretLocation(BWAPI::TilePosition(commandCenter.x + (BWAPI::UnitTypes::Terran_Missile_Turret.tileWidth() * 2), commandCenter.y - BWAPI::UnitTypes::Terran_Missile_Turret.tileHeight())))
			{
				BWAPI::TilePosition turretPosition = BWAPI::TilePosition(commandCenter.x + (BWAPI::UnitTypes::Terran_Missile_Turret.tileWidth() * 2), commandCenter.y - BWAPI::UnitTypes::Terran_Missile_Turret.tileHeight());
				return getPositionNear(BWAPI::UnitTypes::Terran_Missile_Turret, turretPosition, _infoManager.isMech(_infoManager.getStrategy()));
			}
			//Bottom
			else if (validTurretLocation(BWAPI::TilePosition(commandCenter.x - BWAPI::UnitTypes::Terran_Missile_Turret.tileWidth(), commandCenter.y + (BWAPI::UnitTypes::Terran_Missile_Turret.tileHeight() * 2))))
			{
				BWAPI::TilePosition turretPosition = BWAPI::TilePosition(commandCenter.x - BWAPI::UnitTypes::Terran_Missile_Turret.tileWidth(), commandCenter.y + (BWAPI::UnitTypes::Terran_Missile_Turret.tileHeight() * 2));
				return getPositionNear(BWAPI::UnitTypes::Terran_Missile_Turret, turretPosition, _infoManager.isMech(_infoManager.getStrategy()));
			}
			//Upper Left
			else if (validTurretLocation(BWAPI::TilePosition(commandCenter.x - BWAPI::UnitTypes::Terran_Missile_Turret.tileWidth(), commandCenter.y - (BWAPI::UnitTypes::Terran_Missile_Turret.tileHeight() * 2))))
			{
				BWAPI::TilePosition turretPosition = BWAPI::TilePosition(commandCenter.x - BWAPI::UnitTypes::Terran_Missile_Turret.tileWidth(), commandCenter.y - (BWAPI::UnitTypes::Terran_Missile_Turret.tileHeight() * 2));
				return getPositionNear(BWAPI::UnitTypes::Terran_Missile_Turret, turretPosition, _infoManager.isMech(_infoManager.getStrategy()));
			}
			else
			{
				return getPositionNear(BWAPI::UnitTypes::Terran_Missile_Turret, commandCenter, _infoManager.isMech(_infoManager.getStrategy()));
			}
		}
	}

	BWAPI::Position turretsTowardsEnemy = BWAPI::Position(_infoManager.getMainPosition());

	if (_infoManager.getEnemyMainTilePos() != BWAPI::TilePosition(0, 0))
	{
		turretsTowardsEnemy = BWAPI::Position(BWAPI::Position(_infoManager.getMainPosition()).x + (BWAPI::Position(_infoManager.getEnemyMainTilePos()).x - BWAPI::Position(_infoManager.getMainPosition()).x) * .10,
			BWAPI::Position(_infoManager.getMainPosition()).y + (BWAPI::Position(_infoManager.getEnemyMainTilePos()).y - BWAPI::Position(_infoManager.getMainPosition()).y) * .10);
	}

	return getPositionNear(BWAPI::UnitTypes::Terran_Missile_Turret, BWAPI::TilePosition(turretsTowardsEnemy), _infoManager.isMech(_infoManager.getStrategy()));;
}

bool insanitybot::BuildingPlacer::validTurretLocation(BWAPI::TilePosition targetLocation)
{
	int x = targetLocation.x + 1;
	int y = targetLocation.y + 1;
	//Check that the tile's bottom right corner doesn't fall outside of the map
	if (x >= 0 && x < BWAPI::Broodwar->mapWidth() && y >= 0 && y < BWAPI::Broodwar->mapHeight())
	{
		//Check each tilePosition
		if (BWAPI::Broodwar->isBuildable(targetLocation, true) && BWAPI::Broodwar->isBuildable(targetLocation + BWAPI::TilePosition(0, 1), true) &&
			BWAPI::Broodwar->isBuildable(targetLocation + BWAPI::TilePosition(1, 0), true) && BWAPI::Broodwar->isBuildable(targetLocation + BWAPI::TilePosition(1, 1), true))
		{
			return true;
		}
	}

	return false;
}

BWAPI::TilePosition insanitybot::BuildingPlacer::getPositionNear(BWAPI::UnitType building, BWAPI::TilePosition beginingPoint, bool isMech)
{
	//returns a valid build location near the specified tile position.
	//searches outward in a spiral.
	int x = beginingPoint.x;
	int y = beginingPoint.y;
	int length = 1;
	int j = 0;
	bool first = true;
	int dx = 0;
	int dy = 1;
	while (length < BWAPI::Broodwar->mapWidth())
	{
		//if we can build here, return this tile position
		if (x >= 0 && x < BWAPI::Broodwar->mapWidth() && y >= 0 && y < BWAPI::Broodwar->mapHeight())
		{
			bool inChoke = false;

			if (theMap.GetArea(beginingPoint))
			{
				for (auto choke : theMap.GetArea(beginingPoint)->ChokePoints())
				{
					if (abs(choke->Center().x - BWAPI::WalkPosition(BWAPI::TilePosition(x, y)).x) <= 8 && abs(choke->Center().y - BWAPI::WalkPosition(BWAPI::TilePosition(x, y)).y) <= 8)
					{
						inChoke = true;
						break;
					}
				}
			}

			if ((building == BWAPI::UnitTypes::Terran_Factory || building == BWAPI::UnitTypes::Terran_Starport || building == BWAPI::UnitTypes::Terran_Science_Facility) && isMech)
			{
				if (canBuildWithSpace(BWAPI::TilePosition(x, y), building, 1) && !inChoke)
					if (theMap.GetArea(beginingPoint) == theMap.GetArea(BWAPI::TilePosition(x,y)))
						return BWAPI::TilePosition(x, y);
			}
			else
			{
				if (canBuildWithSpace(BWAPI::TilePosition(x, y), building, 0) && !inChoke)
					if (theMap.GetArea(beginingPoint) == theMap.GetArea(BWAPI::TilePosition(x, y)))
						return BWAPI::TilePosition(x, y);
			}
		}

		//otherwise, move to another position
		x = x + dx;
		y = y + dy;
		//count how many steps we take in this direction
		j++;
		if (j == length) { //if we've reached the end, its time to turn
			j = 0;	//reset step counter

			//Spiral out. Keep going.
			if (!first)
				length++; //increment step counter if needed


			first = !first; //first=true for every other turn so we spiral out at the right rate

			//turn counter clockwise 90 degrees:
			if (dx == 0) {
				dx = dy;
				dy = 0;
			}
			else {
				dy = -dx;
				dx = 0;
			}
		}
		//Spiral out. Keep going.
	}

	if (isMech)
		return getPositionNear(building, beginingPoint, false);
	else
		return BWAPI::Broodwar->getBuildLocation(building, beginingPoint);
	//return getPositionNear(building, beginingPoint, "Bio");
}

BuildingPlacer & BuildingPlacer::Instance()
{
	static BuildingPlacer instance;
	return instance;
}