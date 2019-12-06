#include "BWEM1.4.1/src/bwem.h"
#include <BWAPI.h>

namespace insanitybot
{
	class BuildingPlacer
	{
		std::vector< std::vector<bool> > _reserveMap;

	public:
		void initialize();
		void reserveSpaceNearResources();
		BWAPI::TilePosition getDesiredLocation(BWAPI::UnitType building);
	};
}
