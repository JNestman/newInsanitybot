#include "ReadWrite.h"
#include <random>

using namespace insanitybot;

insanitybot::ReadWrite::ReadWrite()
{
}

void insanitybot::ReadWrite::initialize()
{
	buildOrders[BWAPI::Races::Zerg] = { "Nuke", "BioDrops", "FiveFacGol", "OneFacAllIn" };
	buildOrders[BWAPI::Races::Protoss] = { "OneFacAllIn", "Mech", "GreedMech" };
	buildOrders[BWAPI::Races::Terran] = { "MechVT", "MechAllIn", "Mech", "GreedMech" };
	buildOrders[BWAPI::Races::Unknown] = { "OneFacAllIn", "Mech", "MechAllIn" };
}

std::vector<MatchData> insanitybot::ReadWrite::readCompactMatchData()
{
	std::string filePath = "bwapi-data/read/" + BWAPI::Broodwar->enemy()->getName() + ".txt";
	std::vector<MatchData> matches;
	std::ifstream inFile(filePath);
	if (inFile.is_open())
	{
		std::string line;
		while (std::getline(inFile, line))
		{
			std::istringstream iss(line);
			std::string mapName, startPositionStr, buildOrder, resultStr;
			if (std::getline(iss, mapName, '|') &&
				std::getline(iss, startPositionStr, '|') &&
				std::getline(iss, buildOrder, '|') &&
				std::getline(iss, resultStr, '|'))
			{
				// Skip malformed records where build order looks like a coordinate
				// or is otherwise unrecognized
				bool validBuildOrder = false;
				for (auto & race : buildOrders)
				{
					for (auto & order : race.second)
					{
						if (order == buildOrder)
						{
							validBuildOrder = true;
							break;
						}
					}
					if (validBuildOrder) break;
				}
				if (!validBuildOrder)
					continue;

				MatchData match;
				match.mapName = mapName;
				int x, y;
				sscanf(startPositionStr.c_str(), "%d,%d", &x, &y);
				match.startPosition = BWAPI::Position(x, y);
				match.buildOrder = buildOrder;
				match.wonGame = (resultStr == "W");
				matches.push_back(match);
			}
		}
		inFile.close();
	}
	return matches;
}

void insanitybot::ReadWrite::writeCompactMatchData(const std::string& opponentName, const std::string& mapName, const BWAPI::Position& startPosition, std::string buildOrder, bool wonGame)
{
	// Carry forward existing history from the read directory
	std::vector<MatchData> existing = readCompactMatchData();
	//std::string filePath = "bwapi-data/write/" + opponentName + ".txt";
	std::string filePath = "bwapi-data/read/" + opponentName + ".txt";
	std::ofstream outFile(filePath, std::ios::trunc);
	if (outFile.is_open())
	{
		// Write all previous records first
		for (const auto & match : existing)
		{
			outFile << match.mapName << "|"
				<< match.startPosition.x << "," << match.startPosition.y << "|"
				<< match.buildOrder << "|"
				<< (match.wonGame ? 'W' : 'L') << "\n";
		}
		// Then append the new result
		outFile << mapName << "|"
			<< startPosition.x << "," << startPosition.y << "|"
			<< buildOrder << "|"
			<< (wonGame ? 'W' : 'L') << "\n";
		outFile.close();
	}
	else
	{
		BWAPI::Broodwar << "Error opening file for writing: " << filePath << std::endl;
	}
}

std::string insanitybot::ReadWrite::selectBuildOrder(
	const std::vector<MatchData>& matchHistory,
	const std::string& currentMap,
	const BWAPI::Position& currentStartPosition)
{
	struct WL { int w = 0; int l = 0; };
	std::unordered_map<std::string, WL> stats;

	// Aggregate wins and losses per build for this map/start
	for (const auto& match : matchHistory) {
		if (match.mapName == currentMap && match.startPosition == currentStartPosition) {
			auto& s = stats[match.buildOrder];
			match.wonGame ? ++s.w : ++s.l;
		}
	}

	BWAPI::Race enemyRace = BWAPI::Broodwar->enemy()->getRace();
	auto it = buildOrders.find(enemyRace);
	if (it == buildOrders.end() || it->second.empty()) {
		return selectRandomBuildOrder();
	}

	const std::vector<std::string>& candidates = it->second;

	std::vector<double> weights;
	const double explorationFloor = 0.25; // keeps all builds selectable

	for (const auto& build : candidates) {
		int wins = stats.count(build) ? stats[build].w : 0;
		int losses = stats.count(build) ? stats[build].l : 0;
		int totalGames = wins + losses;

		double winRate = totalGames > 0 ? static_cast<double>(wins) / totalGames : 0.5;
		// if never played, assume 50% neutral chance

		double confidence = std::log1p(static_cast<double>(totalGames)); // log(1 + totalGames)
		double weight = explorationFloor + winRate * (1.0 + confidence);

		weights.push_back(weight);

		BWAPI::Broodwar << "[BO] " << build << " wins:" << wins << " losses:" << losses
			<< " winRate:" << winRate << " weight:" << weight << std::endl;
	}

	std::random_device rd;
	std::mt19937 gen(rd());
	std::discrete_distribution<> dist(weights.begin(), weights.end());

	int selectedIndex = dist(gen);
	BWAPI::Broodwar << "Chosen build vs " << enemyRace << ": " << candidates[selectedIndex] << std::endl;
	return candidates[selectedIndex];
}

std::string insanitybot::ReadWrite::selectRandomBuildOrder() {
	// Look up build orders for the opponent's race
	if (buildOrders.find(BWAPI::Broodwar->enemy()->getRace()) != buildOrders.end()) {
		const std::vector<std::string>& orders = buildOrders[BWAPI::Broodwar->enemy()->getRace()];
		// Select a random build order
		int randomIndex = std::rand() % orders.size();
		BWAPI::Broodwar << "Random build selected " << orders[randomIndex] << " chosen." << std::endl;
		return orders[randomIndex];
	}
	else {
		// Fallback case if opponent's race is unknown or not in the map
		return "Mech";  // You can define a default build order
	}
}

std::string insanitybot::ReadWrite::getChosenBuildOrder(BWAPI::Position startPosition)
{
	return selectBuildOrder(readCompactMatchData(), BWAPI::Broodwar->mapFileName(), startPosition);
}


ReadWrite & ReadWrite::Instance()
{
	static ReadWrite instance;
	return instance;
}