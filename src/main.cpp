#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <unistd.h>
#include <ctime>
#include <cstdlib>

using json = nlohmann::json;

// Function prototypes
void newDatabase(const std::string &dbName);
void addPlayer(const std::string &dbName, const std::string &playerName);
void listPlayers(const std::string &dbName);
void startSession(const std::string &dbName);
void renamePlayer(const std::string &dbName, const std::string &oldName, const std::string &newName);
void showStats(const std::string &dbName);

json loadDatabase(const std::string &dbName);
void saveDatabase(const std::string &dbName, const json &db);
std::string getDatabasePath(const std::string &dbName);
void createDatabaseDirectory();
std::string currentDateTime();

double roundTo(double value, int precision) {
    double factor = std::pow(10.0, precision);
    return std::round(value * factor) / factor;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: pokerdb <command> [arguments]\n";
        return 1;
    }

    std::string command = argv[1];

    if (command == "newdb") {
        if (argc != 3) {
            std::cerr << "Usage: pokerdb newdb <dbname>\n";
            return 1;
        }
        newDatabase(argv[2]);
    } else if (command == "addPlayer") {
        if (argc != 4) {
            std::cerr << "Usage: pokerdb addPlayer <dbname> <playerName>\n";
            return 1;
        }
        addPlayer(argv[2], argv[3]);
    } else if (command == "renamePlayer") {
        if (argc != 5) {
            std::cerr << "Usage: pokerdb renamePlayer <dbname> <oldName> <newName>\n";
            return 1;
        }
        renamePlayer(argv[2], argv[3], argv[4]);
    } else if (command == "startSession") {
        if (argc != 3) {
            std::cerr << "Usage: pokerdb startSession <dbname>\n";
            return 1;
        }
        startSession(argv[2]);
    } else if (command == "listPlayers") {
        if (argc != 3) {
            std::cerr << "Usage: pokerdb listPlayers <dbname>\n";
            return 1;
        }
        listPlayers(argv[2]);
    } else if (command == "stats") {
        if (argc != 3) {
            std::cerr << "Usage: pokerdb stats <dbname>\n";
            return 1;
        }
        showStats(argv[2]);
    } else {
        std::cerr << "Unknown command: " << command << "\n";
        return 1;
    }

    return 0;
}

void newDatabase(const std::string &dbName) {
    json db;
    db["players"] = json::array();
    db["sessions"] = json::array();

    saveDatabase(dbName, db);
    std::cout << "Database created: " << dbName << "\n";
}

void addPlayer(const std::string &dbName, const std::string &playerName) {
    json db = loadDatabase(dbName);

    for (const auto &player : db["players"]) {
        if (player["name"] == playerName) {
            std::cerr << "Player already exists: " << playerName << "\n";
            return;
        }
    }

    json newPlayer = {{"name", playerName}, {"profit", 0.0}, {"sessions", 0}, {"hoursPlayed", 0}};
    db["players"].push_back(newPlayer);

    saveDatabase(dbName, db);
    std::cout << "Player added: " << playerName << "\n";
}

void listPlayers(const std::string &dbName) {
    json db = loadDatabase(dbName);

    if (db["players"].empty()) {
        std::cout << "No players in the database.\n";
        return;
    }

    std::cout << "\nList of Players:\n";
    std::cout << "--------------------------\n";
    for (const auto &player : db["players"]) {
        std::cout << "Name: " << player["name"]
                  << ", Profit: " << std::fixed << std::setprecision(2) << roundTo(player["profit"], 2) << "\n";
    }
    std::cout << "--------------------------\n";
}

void renamePlayer(const std::string &dbName, const std::string &oldName, const std::string &newName) {
    json db = loadDatabase(dbName);

    for (auto &player : db["players"]) {
        if (player["name"] == oldName) {
            player["name"] = newName;
            saveDatabase(dbName, db);
            std::cout << "Player renamed to: " << newName << "\n";
            return;
        }
    }

    std::cerr << "Player not found: " << oldName << "\n";
}

bool playerExists(const json &db, const std::string &playerName) {
    for (const auto &player : db["players"]) {
        if (player["name"] == playerName) {
            return true;
        }
    }
    return false;
}

void addPlayerToSession(json &db, json &session) {
    std::string playerName;
    double buyin;

    std::cout << "Enter player name: ";
    std::cin >> playerName;
    std::cout << "Enter buy-in amount: ";
    std::cin >> buyin;

    if (!playerExists(db, playerName)) {
        std::cerr << "Unknown player: " << playerName << "\n";
        return;
    }

    if (session["players"].contains(playerName)) {
        std::cerr << playerName << " is already in the session.\n";
        return;
    }

    session["players"][playerName] = {
        {"buyin", buyin},
        {"rebuys", json::array()},
        {"finalStack", 0.0}
    };
}

void addRebuyToPlayer(json &session) {
    std::string playerName;
    double rebuyAmount;

    std::cout << "Enter player name: ";
    std::cin >> playerName;

    if (!session["players"].contains(playerName)) {
        std::cerr << "Player not in session: " << playerName << "\n";
        return;
    }

    std::cout << "Enter rebuy amount: ";
    std::cin >> rebuyAmount;

    session["players"][playerName]["rebuys"].push_back(rebuyAmount);
}

void addNewPlayerToDatabase(json &db, const std::string &dbName, json &session) {
    std::string newPlayerName;
    std::cout << "Enter new player's name: ";
    std::cin >> newPlayerName;

    if (playerExists(db, newPlayerName)) {
        std::cerr << "Player already exists in the database: " << newPlayerName << "\n";
        return;
    }

    addPlayer(dbName, newPlayerName);  // Use the addPlayer function to add new player to database
    std::cout << "New player " << newPlayerName << " added to the database.\n";

    double buyin;
    std::cout << "Enter buy-in amount for " << newPlayerName << ": ";
    std::cin >> buyin;

    session["players"][newPlayerName] = {
        {"buyin", buyin},
        {"rebuys", json::array()},
        {"finalStack", 0.0}
    };
}

void finalizeSession(json &db, json &session) {
    session["endTime"] = currentDateTime();
    std::cout << "Finalizing session...\n";

    double totalBuyins = 0.0, totalStacks = 0.0;
    std::map<std::string, double> profitMap;

    for (auto &player : session["players"].items()) {
        const std::string &playerName = player.key();
        double buyin = player.value()["buyin"];
        double rebuysSum = std::accumulate(
            player.value()["rebuys"].begin(),
            player.value()["rebuys"].end(),
            0.0,
            [](double sum, const nlohmann::json &rebuy) {
                return sum + rebuy.get<double>();
            });

        std::cout << "Enter final stack for " << playerName << ": ";
        double finalStack;
        std::cin >> finalStack;

        player.value()["finalStack"] = finalStack;

        totalBuyins += roundTo(buyin + rebuysSum, 2);
        totalStacks += roundTo(finalStack, 2);

        double profit = roundTo(finalStack - (buyin + rebuysSum), 2);
        profitMap[playerName] = profit;
    }

    double profitDifference = roundTo(totalStacks - totalBuyins, 2);

    if (profitDifference != 0) {
        std::cout << "Profit difference is not zero (" << profitDifference << "). Re-enter final stacks.\n";
        // Re-enter final stacks until the profits sum to 0
        do {
            totalBuyins = 0.0;
            totalStacks = 0.0;
            profitMap.clear();

            for (auto &player : session["players"].items()) {
                const std::string &playerName = player.key();
                double buyin = player.value()["buyin"];
                double rebuysSum = std::accumulate(
                    player.value()["rebuys"].begin(),
                    player.value()["rebuys"].end(),
                    0.0,
                    [](double sum, const nlohmann::json &rebuy) {
                        return sum + rebuy.get<double>();
                    });

                std::cout << "Re-enter final stack for " << playerName << ": ";
                double finalStack;
                std::cin >> finalStack;

                player.value()["finalStack"] = finalStack;

                totalBuyins += roundTo(buyin + rebuysSum, 2);
                totalStacks += roundTo(finalStack, 2);

                double profit = roundTo(finalStack - (buyin + rebuysSum), 2);
                profitMap[playerName] = profit;
            }

            profitDifference = roundTo(totalStacks - totalBuyins, 2);
        } while (profitDifference != 0);
    }

    // Update database with the valid profits, sessions, and hours
    for (auto &player : session["players"].items()) {
        const std::string &playerName = player.key();
        double buyin = roundTo(player.value()["buyin"], 2);
        double rebuysSum = roundTo(std::accumulate(
            player.value()["rebuys"].begin(),
            player.value()["rebuys"].end(),
            0.0,
            [](double sum, const nlohmann::json &rebuy) {
                return sum + rebuy.get<double>();
            }), 2);

        double finalStack = roundTo(player.value()["finalStack"], 2);
        double profit = roundTo(profitMap[playerName], 2);

        // Update player's profit in the database
        for (auto &dbPlayer : db["players"]) {
            if (dbPlayer["name"] == playerName) {
                dbPlayer["profit"] = roundTo(dbPlayer["profit"].get<double>() + profit, 2);

                // Update the number of sessions played
                dbPlayer["sessions"] = dbPlayer["sessions"].get<int>() + 1;

                // Update hours played based on session duration
                std::string startTime = session["startTime"];
                std::string endTime = session["endTime"];

                struct tm start_tm = {};
                struct tm end_tm = {};
                strptime(startTime.c_str(), "%Y-%m-%d %H:%M:%S", &start_tm);
                strptime(endTime.c_str(), "%Y-%m-%d %H:%M:%S", &end_tm);

                time_t start_time = mktime(&start_tm);
                time_t end_time = mktime(&end_tm);
                double hoursPlayed = roundTo(difftime(end_time, start_time) / 3600.0, 2);

                dbPlayer["hoursPlayed"] = dbPlayer["hoursPlayed"].get<double>() + hoursPlayed;

                break;
            }
        }
    }
}

void startSession(const std::string &dbName) {
    json db = loadDatabase(dbName);

    if (db["players"].empty()) {
        std::cerr << "No players in the database. Add players first.\n";
        return;
    }

    json session;
    session["players"] = json::object();
    session["startTime"] = currentDateTime();
    session["endTime"] = "";

    std::cout << "Starting new session...\n";

    while (true) {
        int option = 0;
        std::cout << "\nOptions:\n"
                  << "1. Add Player to Session\n"
                  << "2. Add Rebuy for a Player\n"
                  << "3. Add New Player to Database\n"
                  << "4. Finalize Session\n"
                  << "Choose an option: ";
        std::cin >> option;

        if (option == 1) {
            addPlayerToSession(db, session);
        } else if (option == 2) {
            addRebuyToPlayer(session);
        } else if (option == 3) {
            addNewPlayerToDatabase(db, dbName, session);
        } else if (option == 4) {
            finalizeSession(db, session);
            break;
        } else {
            std::cerr << "Invalid option, please try again.\n";
        }
    }

    db["sessions"].push_back(session);
    saveDatabase(dbName, db);
}

void showStats(const std::string &dbName) {
    json db = loadDatabase(dbName);
    std::cout << "Statistics:\n";
    std::cout << "--------------------------\n";
    // TODO: Profits and other stats from players
    std::cout << "--------------------------\n";
}

json loadDatabase(const std::string &dbName) {
    std::string path = getDatabasePath(dbName);
    std::ifstream input(path);

    if (!input) {
        std::cerr << "Could not open database file: " << path << "\n";
        exit(1);
    }

    json db;
    input >> db;
    return db;
}

void saveDatabase(const std::string &dbName, const json &db) {
    std::string path = getDatabasePath(dbName);
    std::ofstream output(path);

    if (!output) {
        std::cerr << "Could not save to database file: " << path << "\n";
        exit(1);
    }

    output << db.dump(4);
}

std::string getDatabasePath(const std::string &dbName) {
    std::string configDir = getenv("HOME");
    configDir += "/.config/pokerdb/";

    createDatabaseDirectory();

    return configDir + dbName + ".json";
}

void createDatabaseDirectory() {
    std::string dir = getenv("HOME") + std::string("/.config/pokerdb");
    struct stat st = {0};

    if (stat(dir.c_str(), &st) == -1) {
        mkdir(dir.c_str(), 0700);
    }
}

std::string currentDateTime() {
    std::time_t now = std::time(nullptr);
    std::tm *tmNow = std::localtime(&now);
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tmNow);
    return std::string(buffer);
}
