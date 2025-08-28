#ifndef READWRITE_H
#define READWRITE_H
//#include <iostream>
#include "InformationManager.h"

struct MatchData {
	std::string mapName;
	BWAPI::Position startPosition;
	std::string buildOrder;
	bool wonGame;
};

namespace insanitybot
{
	class ReadWrite
	{
		std::map<BWAPI::Race, std::vector<std::string>> buildOrders;

	public:
		ReadWrite();
		~ReadWrite() {};
		void initialize();

		std::vector<MatchData> readCompactMatchData();
		void  writeCompactMatchData(const std::string& opponentName, const std::string& mapName, const BWAPI::Position& startPosition, std::string buildOrder, bool wonGame);
		std::string selectBuildOrder(const std::vector<MatchData>& matchHistory, const std::string& currentMap, const BWAPI::Position& currentStartPosition);
		std::string selectRandomBuildOrder();

		std::string getChosenBuildOrder(BWAPI::Position startPosition);


		static ReadWrite & Instance();
	};
}

#endif