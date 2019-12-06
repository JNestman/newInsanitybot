#include <list>
#include <BWAPI.h>
#pragma once


namespace insanitybot
{
	class Build
	{
		BWAPI::Player										_self;
		BWAPI::Player										_enemy;
		bool												_initialBarracks;

	public:
		Build();
		~Build() {};
		void initialize();
		void update();

		std::list<BWAPI::UnitType>							_queue;
	};
}