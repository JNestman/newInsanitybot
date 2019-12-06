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

		static GameCommander & Instance();
	};
}