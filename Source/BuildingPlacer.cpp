#include "BuildingPlacer.h"

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
				BWAPI::TilePosition minTile = mineral.getTilePosition();
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

			for (const auto gas : base->getGeysers())
			{
				// Don't build on the right edge or a right corner of a geyser.
				// It causes workers to take a long path around. Other locations are OK.
				BWAPI::TilePosition gasTile = gas->getTilePosition();
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

BWAPI::TilePosition BuildingPlacer::getDesiredLocation(BWAPI::UnitType building)
{	
	return BWAPI::TilePosition(0, 0);
}