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

BWAPI::TilePosition BuildingPlacer::getDesiredLocation(BWAPI::UnitType building)
{	
	BWAPI::TilePosition desiredLocation = BWAPI::Broodwar->getBuildLocation(building, InformationManager::Instance().getMainPosition());
	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType().isBuilding())
		{

		}
	}
	return desiredLocation;
}