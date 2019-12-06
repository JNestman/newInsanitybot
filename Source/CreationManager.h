#include "BWEM1.4.1/src/bwem.h"
#include <BWAPI.h>
#include <list>
#include "WorkerManager.h"

namespace insanitybot
{
	class CreationManager
	{
		BWAPI::Player										_self;
	public:
		CreationManager();
		~CreationManager() {};

		void update(std::list<BWAPI::Unit> _producers, std::list<BWAPI::Unit> _commandCenters, std::list<BWAPI::Unit> _workers
					, int numWorkersWanted, std::list<BWAPI::UnitType>& queue);
	};
}
