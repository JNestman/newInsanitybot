#include "UnitManager.h"
#include "InformationManager.h"
#include "CreationManager.h"

namespace insanitybot
{
	class GameCommander
	{
		UnitManager			_unitManager;
		WorkerManager		_workerManager;
		InformationManager	_informationManager;
		CreationManager		_creationManager;


	public:
		GameCommander();
		~GameCommander() {};
		void initialize();
		void update();

		void onUnitShow(BWAPI::Unit unit);
		void onUnitCreate(BWAPI::Unit unit);
		void onUnitDestroy(BWAPI::Unit unit);
		void onUnitRenegade(BWAPI::Unit unit);
		void onUnitComplete(BWAPI::Unit unit);

		void infoText();

		static GameCommander & Instance();
	};
}