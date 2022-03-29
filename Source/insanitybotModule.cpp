#include "insanitybotModule.h"
#include "BWEM1.4.1/src/bwem.h"

using namespace BWAPI;
using namespace Filter;
//using namespace BWEM;
//using namespace BWEM::BWAPI_ext;
//using namespace BWEM::utils;

namespace { auto & theMap = BWEM::Map::Instance(); }

using namespace insanitybot;

insanitybot::insanitybotModule::insanitybotModule()
{
}

void insanitybotModule::onStart()
{
	try
	{
		// Hello World!
		Broodwar->sendText("Hello world!");

		// Print the map name.
		// BWAPI returns std::string when retrieving a string, don't forget to add .c_str() when printing!
		Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;
	
		// Enable the UserInput flag, which allows us to control the bot and type messages.
		Broodwar->enableFlag(Flag::UserInput);

		// Uncomment the following line and the bot will know about everything through the fog of war (cheat).
		//Broodwar->enableFlag(Flag::CompleteMapInformation);

		// Set the command optimization level so that common commands can be grouped
		// and reduce the bot's APM (Actions Per Minute).
		Broodwar->setCommandOptimizationLevel(2);

		// Check if this is a replay
		if ( Broodwar->isReplay() )
		{
			// Announce the players in the replay
			Broodwar << "The following players are in this replay:" << std::endl;
    
			// Iterate all the players in the game using a std:: iterator
			Playerset players = Broodwar->getPlayers();
			for(auto p : players)
			{
			  // Only print the player if they are not an observer
			  if ( !p->isObserver() )
				Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
			}

		}
		else // if this is not a replay
		{
			// Retrieve you and your enemy's races. enemy() will just return the first enemy.
			// If you wish to deal with multiple enemies then you must use enemies().
			if ( Broodwar->enemy() ) // First make sure there is an enemy
				Broodwar << "The matchup is " << Broodwar->self()->getRace() << " vs " << Broodwar->enemy()->getRace() << std::endl;

			Broodwar << "Map initialization..." << std::endl;

			theMap.Initialize();
			theMap.EnableAutomaticPathAnalysis();
			bool startingLocationsOK = theMap.FindBasesForStartingLocations();
			assert(startingLocationsOK);

			Broodwar << "glhf" << std::endl;

			Broodwar->setLocalSpeed(5);

			commander.initialize();
		}
	}
	catch (const std::exception & e)
	{
		Broodwar << "EXCEPTION: " << e.what() << std::endl;
	}
  

}

void insanitybotModule::onEnd(bool isWinner)
{
  // Called when the game ends
  if ( isWinner )
  {
    // Log your win here!
  }
}

void insanitybotModule::onFrame()
{
  // Called once every game frame
	try
	{
		//BWEM::utils::gridMapExample(theMap);
		//BWEM::utils::drawMap(theMap);

		// Display the game frame rate as text in the upper left area of the screen
		Broodwar->drawTextScreen(200, 0, "FPS: %d", Broodwar->getFPS());
		Broodwar->drawTextScreen(200, 10, "Average FPS: %f", Broodwar->getAverageFPS());
		//if (BWAPI::Broodwar->getFrameCount() % 100 == 0)
		//	Broodwar->sendText("Frame: %d", BWAPI::Broodwar->getFrameCount());
		commander.infoText();

		// Return if the game is a replay or is paused
		if (Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self())
			return;

		// Prevent spamming by only running our onFrame once every number of latency frames.
		// Latency frames are the number of frames before commands are processed.
		if (Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0)
			return;

		///////////////////////////////////////
		//Everything for GameCommander is here
		//////////////////////////////////////
		commander.update();
	}
	catch (const std::exception & e)
	{
		Broodwar << "EXCEPTION: " << e.what() << std::endl;
	}
}

void insanitybotModule::onSendText(std::string text)
{
	BWEM::utils::MapDrawer::ProcessCommand(text);

	//send the text to the game if it is not being processed.
	Broodwar->sendText("%s", text.c_str());


	// Make sure to use %s and pass the text as a parameter,
	// otherwise you may run into problems when you use the %(percent) character!
}

void insanitybotModule::onReceiveText(BWAPI::Player player, std::string text)
{
	// Parse the received text
	Broodwar << player->getName() << " said \"" << text << "\"" << std::endl;
}

void insanitybotModule::onPlayerLeft(BWAPI::Player player)
{
	// Interact verbally with the other players in the game by
	// announcing that the other player has left.
	Broodwar->sendText("Goodbye %s!", player->getName().c_str());
}

void insanitybotModule::onNukeDetect(BWAPI::Position target)
{
  // Check if the target is a valid position
  if ( target )
  {
    // if so, print the location of the nuclear strike target
    Broodwar << "Nuclear Launch Detected at " << target << std::endl;
  }
  else 
  {
    // Otherwise, ask other players where the nuke is!
    Broodwar->sendText("Where's the nuke?");
  }

  // You can also retrieve all the nuclear missile targets using Broodwar->getNukeDots()!
}

void insanitybotModule::onUnitDiscover(BWAPI::Unit unit)
{
}

void insanitybotModule::onUnitEvade(BWAPI::Unit unit)
{
}

void insanitybotModule::onUnitShow(BWAPI::Unit unit)
{
	commander.onUnitShow(unit);
}

void insanitybotModule::onUnitHide(BWAPI::Unit unit)
{
}

void insanitybotModule::onUnitCreate(BWAPI::Unit unit)
{
  if ( Broodwar->isReplay() )
  {
    // if we are in a replay, then we will print out the build order of the structures
    if ( unit->getType().isBuilding() && !unit->getPlayer()->isNeutral() )
    {
      int seconds = Broodwar->getFrameCount()/24;
      int minutes = seconds/60;
      seconds %= 60;
      Broodwar->sendText("%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
    }
  }
}

void insanitybotModule::onUnitDestroy(BWAPI::Unit unit)
{
	try
	{
		if (unit->getType().isMineralField())    theMap.OnMineralDestroyed(unit);
		else if (unit->getType().isSpecialBuilding()) theMap.OnStaticBuildingDestroyed(unit);
	}
	catch (const std::exception & e)
	{
		Broodwar << "EXCEPTION: " << e.what() << std::endl;
	}

	commander.onUnitDestroy(unit);
}

void insanitybotModule::onUnitMorph(BWAPI::Unit unit)
{
  if ( Broodwar->isReplay() )
  {
    // if we are in a replay, then we will print out the build order of the structures
    if ( unit->getType().isBuilding() && !unit->getPlayer()->isNeutral() )
    {
      int seconds = Broodwar->getFrameCount()/24;
      int minutes = seconds/60;
      seconds %= 60;
      Broodwar->sendText("%.2d:%.2d: %s morphs a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
    }
  }
}

void insanitybotModule::onUnitRenegade(BWAPI::Unit unit)
{
	commander.onUnitRenegade(unit);
}

void insanitybotModule::onSaveGame(std::string gameName)
{
  Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void insanitybotModule::onUnitComplete(BWAPI::Unit unit)
{
}
