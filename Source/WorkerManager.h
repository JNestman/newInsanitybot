#include "BWEM1.4.1/src/bwem.h"
#include <BWAPI.h>
#include <list>

using namespace BWAPI;
using namespace Filter;

namespace insanitybot
{
	class WorkerManager
	{
	public:
		void update(std::list<BWAPI::Unit> _workers, int numProducers);
		void construct(std::list<BWAPI::Unit> _workers, BWAPI::UnitType structure, BWAPI::TilePosition targetLocation);

		static WorkerManager & Instance();
	};
}