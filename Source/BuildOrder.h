#ifndef BUILDORDER_H
#define BUILDORDER_H

#include "BWEM1.4.1/src/bwem.h"

namespace insanitybot
{
	class InformationManager;

	class BuildOrder
	{
		BWAPI::Player										_self;
		BWAPI::Player										_enemy;

		std::string											_initialStrategy;

	public:
		BuildOrder();
		~BuildOrder() {};
		void initialize(std::string strategy);

		void setInitialStrategy(std::string strategy)		{ _initialStrategy = strategy; };

		void SKTerran(InformationManager & _infoManager);
		void Nuke(InformationManager & _infoManager);
		void BioDrops(InformationManager & _infoManager);
		void Mech(InformationManager & _infoManager);
		void MechVT(InformationManager & _infoManager);
		void FiveFacGol(InformationManager & _infoManager);
		void BCMeme(InformationManager & _infoManager);
		void EightRaxDef(InformationManager & _infoManager);
		void OneBaseMech(InformationManager & _infoManager);
		void OneFacAllIn(InformationManager & _infoManager);
		void MechAllIn(InformationManager & _infoManager);
		
		static BuildOrder & Instance();
	};
}

#endif