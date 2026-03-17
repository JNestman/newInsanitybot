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

std::vector<MatchData>  insanitybot::ReadWrite::readCompactMatchData() {
	std::string filePath = "bwapi-data/read/" + BWAPI::Broodwar->enemy()->getName() + ".txt";
	//std::string filePath = "bwapi-data/write/" + BWAPI::Broodwar->enemy()->getName() + ".txt";
	std::vector<MatchData> matches;

	std::ifstream inFile(filePath);
	if (inFile.is_open()) {
		std::string line;
		while (std::getline(inFile, line)) {
			std::istringstream iss(line);
			std::string mapName, startPositionStr, buildOrder, resultStr;

			if (iss >> mapName >> startPositionStr >> buildOrder >> resultStr) {
				MatchData match;
				// Parse map name
				match.mapName = mapName;

				// Parse starting position
				int x, y;
				sscanf(startPositionStr.c_str(), "%d,%d", &x, &y);
				match.startPosition = BWAPI::Position(x, y);

				// Parse build order
				match.buildOrder = buildOrder;

				// Parse result
				match.wonGame = (resultStr == "W");

				matches.push_back(match);
			}
		}
		inFile.close();
	}

	return matches;
}

void  insanitybot::ReadWrite::writeCompactMatchData(const std::string& opponentName, const std::string& mapName, const BWAPI::Position& startPosition, std::string buildOrder, bool wonGame) {
	std::string filePath = "bwapi-data/write/" + opponentName + ".txt";

	std::ofstream outFile(filePath, std::ios::app);
	if (outFile.is_open()) {
		// Write compact format: "mapId startPosition buildOrder result"
		outFile << mapName << " ";
		outFile << startPosition.x << "," << startPosition.y << " ";
		outFile << buildOrder << " ";
		outFile << (wonGame ? 'W' : 'L') << "\n";
		outFile.close();
	}
	else {
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
		return "mech";  // You can define a default build order
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