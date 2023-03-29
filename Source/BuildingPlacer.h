#ifndef BUILDINGPLACER_H
#define BUILDINGPLACER_H

#include "BWEM1.4.1/src/bwem.h"
#include <BWAPI.h>
#include "InformationManager.h"

namespace insanitybot
{
	class BuildingPlacer
	{
		std::vector< std::vector<bool> > _reserveMap;

	public:
		void initialize();
		void reserveSpaceNearResources();
		void reserveTiles(BWAPI::TilePosition position, int width, int 
		);
		void setReserve(BWAPI::TilePosition position, int width, int height, bool flag);
		bool boxOverlapsBase(int x1, int y1, int x2, int y2) const;
		bool tileBlocksAddon(BWAPI::TilePosition position) const;

		bool freeTile(int x, int y) const;
		bool freeOnTop(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const;
		bool freeOnRight(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const;
		bool freeOnLeft(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const;
		bool freeOnBottom(const BWAPI::TilePosition & tile, BWAPI::UnitType buildingType) const;
		bool freeOnAllSides(BWAPI::Unit building) const;

		bool canBuildWithSpace(const BWAPI::TilePosition & position, const BWAPI::UnitType & b, int extraSpace) const;

		
		BWAPI::TilePosition getDesiredLocation(BWAPI::UnitType building, InformationManager & _infoManager, std::list<BWAPI::TilePosition>);
		BWAPI::TilePosition getSupplyLocation(BWAPI::UnitType building, InformationManager & _infoManager);
		BWAPI::TilePosition getTurretLocation(InformationManager & _infoManager);

		bool validTurretLocation(BWAPI::TilePosition targetLocation);

		BWAPI::TilePosition getPositionNear(BWAPI::UnitType building, BWAPI::TilePosition beginingPoint, bool isMech);

		static BuildingPlacer & Instance();
	};
}

#endif