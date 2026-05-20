#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <vector>
#include <sstream>
#include <array>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <httplib.h>
#include <nlohmann/json.hpp>  // JSON library: https://github.com/nlohmann/json
#include <string>
#include <signal.h>
#include <csignal>
#include <atomic>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include "Database.hpp"
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

std::atomic<bool> running(true);


bool callArbiter(const std::string& requestID, int maxRetries = 5, int waitSeconds = 2) {
    httplib::Client client("arbiter-api", 8000); // use service name and container port
    httplib::Headers headers = {
        {"x-key", "taw"}
    };

    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        auto res = client.Post(("/models/create/" + requestID).c_str(), headers, "", "application/json");

        if (res && res->status >= 200 && res->status < 300) {
            std::cout << "Successfully called arbiter-api! Status: " << res->status << std::endl;
            return true;
        }

        std::cerr << "Attempt " << attempt << " failed";
        if (res) {
            std::cerr << ", status: " << res->status;
        } else {
            std::cerr << ", no response";
        }
        std::cerr << ". Retrying in " << waitSeconds << "s..." << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(waitSeconds));
    }

    std::cerr << "Failed to call arbiter-api after " << maxRetries << " retries." << std::endl;
    return false;
}

class ChessPosition {
private:
    std::array<std::array<char, 8>, 8> board;
    bool whiteToMove = true;
    bool whiteKingSideCastle = true;
    bool whiteQueenSideCastle = true;
    bool blackKingSideCastle = true;
    bool blackQueenSideCastle = true;
    int enPassantFile = -1; // -1 if no en passant possible

public:
    ChessPosition() {
        // Initialize starting position
        resetToStartingPosition();
    }

    void resetToStartingPosition() {
        // Initialize empty board
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                board[i][j] = '.';
            }
        }

        // Set up starting position
        // White pieces
        board[0][0] = 'R'; board[0][1] = 'N'; board[0][2] = 'B'; board[0][3] = 'Q';
        board[0][4] = 'K'; board[0][5] = 'B'; board[0][6] = 'N'; board[0][7] = 'R';
        for (int i = 0; i < 8; i++) board[1][i] = 'P';

        // Black pieces
        board[7][0] = 'r'; board[7][1] = 'n'; board[7][2] = 'b'; board[7][3] = 'q';
        board[7][4] = 'k'; board[7][5] = 'b'; board[7][6] = 'n'; board[7][7] = 'r';
        for (int i = 0; i < 8; i++) board[6][i] = 'p';

        whiteToMove = true;
        whiteKingSideCastle = blackKingSideCastle = true;
        whiteQueenSideCastle = blackQueenSideCastle = true;
        enPassantFile = -1;
    }

    std::pair<int, int> findKing(bool isWhite) const {
        char king = isWhite ? 'K' : 'k';
        for (int rank = 0; rank < 8; rank++) {
            for (int file = 0; file < 8; file++) {
                if (board[rank][file] == king) {
                    return { rank, file };
                }
            }
        }
        return { -1, -1 }; // Should never happen in valid position
    }

    bool isSquareAttacked(int rank, int file, bool byWhite) const {
        // Check for pawn attacks
        int pawnDirection = byWhite ? 1 : -1;
        int pawnRank = rank - pawnDirection;
        char pawn = byWhite ? 'P' : 'p';

        if (pawnRank >= 0 && pawnRank < 8) {
            if (file > 0 && board[pawnRank][file - 1] == pawn) return true;
            if (file < 7 && board[pawnRank][file + 1] == pawn) return true;
        }

        // Check for knight attacks
        char knight = byWhite ? 'N' : 'n';
        int knightMoves[8][2] = { {-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1} };
        for (int i = 0; i < 8; i++) {
            int newRank = rank + knightMoves[i][0];
            int newFile = file + knightMoves[i][1];
            if (newRank >= 0 && newRank < 8 && newFile >= 0 && newFile < 8) {
                if (board[newRank][newFile] == knight) return true;
            }
        }

        // Check for bishop/queen diagonal attacks
        char bishop = byWhite ? 'B' : 'b';
        char queen = byWhite ? 'Q' : 'q';
        int directions[4][2] = { {1,1},{1,-1},{-1,1},{-1,-1} };

        for (int d = 0; d < 4; d++) {
            for (int i = 1; i < 8; i++) {
                int newRank = rank + directions[d][0] * i;
                int newFile = file + directions[d][1] * i;

                if (newRank < 0 || newRank >= 8 || newFile < 0 || newFile >= 8) break;

                char piece = board[newRank][newFile];
                if (piece != '.') {
                    if (piece == bishop || piece == queen) return true;
                    break; // Blocked by another piece
                }
            }
        }

        // Check for rook/queen straight attacks
        char rook = byWhite ? 'R' : 'r';
        int straightDirections[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };

        for (int d = 0; d < 4; d++) {
            for (int i = 1; i < 8; i++) {
                int newRank = rank + straightDirections[d][0] * i;
                int newFile = file + straightDirections[d][1] * i;

                if (newRank < 0 || newRank >= 8 || newFile < 0 || newFile >= 8) break;

                char piece = board[newRank][newFile];
                if (piece != '.') {
                    if (piece == rook || piece == queen) return true;
                    break; // Blocked by another piece
                }
            }
        }

        // Check for king attacks
        char king = byWhite ? 'K' : 'k';
        for (int dr = -1; dr <= 1; dr++) {
            for (int df = -1; df <= 1; df++) {
                if (dr == 0 && df == 0) continue;
                int newRank = rank + dr;
                int newFile = file + df;
                if (newRank >= 0 && newRank < 8 && newFile >= 0 && newFile < 8) {
                    if (board[newRank][newFile] == king) return true;
                }
            }
        }

        return false;
    }

    bool isInCheck(bool isWhiteKing) const {
        auto kingPos = findKing(isWhiteKing);
        if (kingPos.first == -1) return false; // No king found

        return isSquareAttacked(kingPos.first, kingPos.second, !isWhiteKing);
    }

    bool makeMove(const std::string& move) {
        if (move.length() < 4) return false;

        int fromFile = move[0] - 'a';
        int fromRank = move[1] - '1';
        int toFile = move[2] - 'a';
        int toRank = move[3] - '1';

        if (fromFile < 0 || fromFile > 7 || fromRank < 0 || fromRank > 7 ||
            toFile < 0 || toFile > 7 || toRank < 0 || toRank > 7) {
            return false;
        }

        char piece = board[fromRank][fromFile];
        if (piece == '.') return false;

        // Handle en passant capture
        if ((piece == 'P' || piece == 'p') && toFile == enPassantFile &&
            ((piece == 'P' && fromRank == 4 && toRank == 5) ||
                (piece == 'p' && fromRank == 3 && toRank == 2))) {
            // En passant capture
            int capturedPawnRank = piece == 'P' ? 4 : 3;
            board[capturedPawnRank][toFile] = '.';
        }

        // Set en passant flag for next move
        enPassantFile = -1;
        if ((piece == 'P' && fromRank == 1 && toRank == 3) ||
            (piece == 'p' && fromRank == 6 && toRank == 4)) {
            enPassantFile = fromFile;
        }

        // Handle castling
        if (piece == 'K' && fromFile == 4 && fromRank == 0) {
            if (toFile == 6 && toRank == 0) { // King-side castling
                board[0][5] = board[0][7]; // Move rook
                board[0][7] = '.';
            }
            else if (toFile == 2 && toRank == 0) { // Queen-side castling
                board[0][3] = board[0][0]; // Move rook
                board[0][0] = '.';
            }
            whiteKingSideCastle = whiteQueenSideCastle = false;
        }
        else if (piece == 'k' && fromFile == 4 && fromRank == 7) {
            if (toFile == 6 && toRank == 7) { // King-side castling
                board[7][5] = board[7][7]; // Move rook
                board[7][7] = '.';
            }
            else if (toFile == 2 && toRank == 7) { // Queen-side castling
                board[7][3] = board[7][0]; // Move rook
                board[7][0] = '.';
            }
            blackKingSideCastle = blackQueenSideCastle = false;
        }

        // Update castling rights
        if (piece == 'K') whiteKingSideCastle = whiteQueenSideCastle = false;
        if (piece == 'k') blackKingSideCastle = blackQueenSideCastle = false;
        if (piece == 'R') {
            if (fromFile == 0 && fromRank == 0) whiteQueenSideCastle = false;
            if (fromFile == 7 && fromRank == 0) whiteKingSideCastle = false;
        }
        if (piece == 'r') {
            if (fromFile == 0 && fromRank == 7) blackQueenSideCastle = false;
            if (fromFile == 7 && fromRank == 7) blackKingSideCastle = false;
        }

        // Handle promotion
        char promotionPiece = piece;
        if (move.length() == 5) {
            char promoPiece = move[4];
            if (piece == 'P') {
                switch (promoPiece) {
                case 'q': promotionPiece = 'Q'; break;
                case 'r': promotionPiece = 'R'; break;
                case 'b': promotionPiece = 'B'; break;
                case 'n': promotionPiece = 'N'; break;
                }
            }
            else if (piece == 'p') {
                switch (promoPiece) {
                case 'q': promotionPiece = 'q'; break;
                case 'r': promotionPiece = 'r'; break;
                case 'b': promotionPiece = 'b'; break;
                case 'n': promotionPiece = 'n'; break;
                }
            }
        }

        // Make the move
        board[fromRank][fromFile] = '.';
        board[toRank][toFile] = promotionPiece;

        whiteToMove = !whiteToMove;
        return true;
    }


    bool kingSideCastle() {
        return whiteToMove ? whiteKingSideCastle : blackKingSideCastle;
    }

    bool queenSideCastle() {
        return whiteToMove ? whiteQueenSideCastle : blackQueenSideCastle;
    }
};

class StockfishEngine {
private:
    int stdinPipe[2];
    int stdoutPipe[2];
    pid_t childPid = -1;
    bool engineRunning = false;

public:
    ~StockfishEngine() {
        if (engineRunning) {
            sendCommand("quit");
            if (childPid > 0) {
                // Give the process some time to exit gracefully
                usleep(100000); // 100ms
                int status;
                if (waitpid(childPid, &status, WNOHANG) == 0) {
                    // Process still running, force kill
                    kill(childPid, SIGTERM);
                    waitpid(childPid, &status, 0);
                }
            }
            if (stdinPipe[1] != -1) close(stdinPipe[1]);
            if (stdoutPipe[0] != -1) close(stdoutPipe[0]);
        }
    }

    bool init(const std::string& path = "stockfish") {
        // Create pipes for stdin and stdout
        if (pipe(stdinPipe) == -1 || pipe(stdoutPipe) == -1) {
            std::cerr << "Failed to create pipes" << std::endl;
            return false;
        }

        // Fork the process
        childPid = fork();
        if (childPid == -1) {
            std::cerr << "Failed to fork process" << std::endl;
            close(stdinPipe[0]); close(stdinPipe[1]);
            close(stdoutPipe[0]); close(stdoutPipe[1]);
            return false;
        }

        if (childPid == 0) {
            // Child process - setup pipes and exec stockfish
            
            // Redirect stdin to read from our pipe
            dup2(stdinPipe[0], STDIN_FILENO);
            close(stdinPipe[0]);
            close(stdinPipe[1]);

            // Redirect stdout to write to our pipe
            dup2(stdoutPipe[1], STDOUT_FILENO);
            close(stdoutPipe[0]);
            close(stdoutPipe[1]);

            // Execute stockfish
            execl(path.c_str(), "stockfish", (char*)nullptr);
            
            // If we get here, exec failed
            std::cerr << "Failed to execute stockfish at: " << path << std::endl;
            _exit(1);
        }

        // Parent process - close unused pipe ends
        close(stdinPipe[0]);  // We don't read from stdin pipe
        close(stdoutPipe[1]); // We don't write to stdout pipe

        // Make stdout pipe non-blocking for reading
        int flags = fcntl(stdoutPipe[0], F_GETFL, 0);
        fcntl(stdoutPipe[0], F_SETFL, flags | O_NONBLOCK);

        engineRunning = true;

        // Initialize UCI
        sendCommand("uci");
        std::string line;
        while ((line = readLine()) != "uciok" && !line.empty()) {}

        sendCommand("setoption name Threads value 16");
        sendCommand("setoption name Hash value 16384");

        sendCommand("isready");
        while ((line = readLine()) != "readyok" && !line.empty()) {}

        return true;
    }

    void sendCommand(const std::string& cmd) {
        if (!engineRunning) return;
        std::string command = cmd + "\n";
        ssize_t written = write(stdinPipe[1], command.c_str(), command.length());
        if (written == -1) {
            std::cerr << "Failed to write to stockfish" << std::endl;
        }
    }

    std::string readLine() {
        std::string line;
        char ch;
        ssize_t bytesRead;

        while (true) {
            bytesRead = read(stdoutPipe[0], &ch, 1);
            
            if (bytesRead == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No data available, sleep briefly and try again
                    usleep(1000); // 1ms
                    continue;
                }
                // Other error
                break;
            } else if (bytesRead == 0) {
                // EOF - process likely terminated
                break;
            }

            if (ch == '\n') {
                break;
            } else if (ch != '\r') {
                line += ch;
            }
        }

        return line;
    }

    double* evaluate(std::string depth, bool isWhiteToMove = true) {
        sendCommand("go perft 1");

        std::string text = "";
        std::string line = readLine();
        int count = 0;

        while (true) {
            line = readLine();
            text += line;

            if (line.empty() || line == "") {
                break;
            }
        }
       
        for (char c : text) {
            if (c == ':') {
                count++;
            }
        }

        double* arr = new double[2];
        arr[1] = count - 1;

        // NOTE: The vision was to go to depth 20-22 ( which are commonly used in game reviews ) but for the lack of the power of calculation and for the main purpose of the project ( to be shipped fast ), unfortunately, I had to limit the depth to 10. 
        sendCommand("go depth "+ depth);
        
        int evalScore = 0;
        bool mateFound = false;
        int mateIn = 0;

        // --- Parse Stockfish's output ---
        // Read lines until we see "bestmove", which signals the end of the search.
        while (true) {
            std::string line = readLine();

            // Look for evaluation in centipawns (e.g., "info ... score cp 135 ...")
            std::size_t cpPos = line.find("score cp ");
            if (cpPos != std::string::npos) {
                // Extract the number after "score cp ".
                evalScore = std::stoi(line.substr(cpPos + 9));
                mateFound = false;
            }

            // Look for a mate score (e.g., "info ... score mate 5 ...")
            std::size_t matePos = line.find("score mate ");
            if (matePos != std::string::npos) {
                // Extract the number of moves until mate.
                mateIn = std::stoi(line.substr(matePos + 11));
                mateFound = true;
            }

            // Stop when "bestmove" appears — that means Stockfish has finished.
            if (line.rfind("bestmove", 0) == 0) {
                break;
            }
        }

        arr[0] = evalScore;
        return arr;
    }

    std::string getFenPosition() {
        sendCommand("d");
        std::string fenLine;
        for (;;) {
            std::string line = readLine();
            if (line.find("Fen: ") != std::string::npos) {
                fenLine = line.substr(line.find("Fen: ") + 5); // extract after "Fen: "
                return fenLine;
            }
        }

        return "";
    }
};


struct GameData {
    std::string gameId;
    std::vector<std::string> moves;
    std::vector<int> whiteTimestamps;
    std::vector<int> blackTimestamps;
};

class GameAnalyzer {
private:
    StockfishEngine engine;

    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r\"");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r\"");
        return str.substr(first, (last - first + 1));
    }

    std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        while (std::getline(ss, token, delimiter)) {
            tokens.push_back(trim(token));
        }
        return tokens;
    }

    std::vector<int> parseTimestamps(const std::string& arrayStr) {
        std::vector<int> timestamps;
        std::string cleaned = arrayStr;

        // Remove brackets and quotes
        size_t start = cleaned.find('[');
        size_t end = cleaned.find(']');
        if (start != std::string::npos && end != std::string::npos) {
            cleaned = cleaned.substr(start + 1, end - start - 1);
        }

        auto tokens = split(cleaned, ',');
        for (const auto& token : tokens) {
            if (!token.empty()) {
                timestamps.push_back(std::stoi(token));
            }
        }
        return timestamps;
    }

    std::vector<std::string> parseMoves(const std::string& arrayStr) {
        std::vector<std::string> moves;
        std::string cleaned = arrayStr;

        // Remove brackets
        size_t start = cleaned.find('[');
        size_t end = cleaned.find(']');
        if (start != std::string::npos && end != std::string::npos) {
            cleaned = cleaned.substr(start + 1, end - start - 1);
        }

        auto tokens = split(cleaned, ',');
        for (const auto& token : tokens) {
            if (!token.empty()) {
                moves.push_back(token);
            }
        }
        return moves;
    }

public:
    bool init(const std::string& path = "stockfish") {
        return engine.init(path);
    }

    std::vector<GameData> parseGameFile(const std::string& filename) {
        std::vector<GameData> games;
        std::ifstream file(filename);

        if (!file.is_open()) {
            std::cout << "Error: Could not open " << filename << std::endl;
            return games;
        }

        std::string line, content;
        while (std::getline(file, line)) {
            content += line + "\n";
        }
        file.close();

        // Simple JSON parsing for the specific structure
        size_t pos = 0;
        while ((pos = content.find('"', pos)) != std::string::npos) {
            size_t idStart = pos + 1;
            size_t idEnd = content.find('"', idStart);
            if (idEnd == std::string::npos) break;

            std::string gameId = content.substr(idStart, idEnd - idStart);
            if (gameId.length() < 10) { // Skip short strings like field names
                pos = idEnd + 1;
                continue;
            }

            GameData game;
            game.gameId = gameId;

            // Find the game data block
            size_t blockStart = content.find('{', idEnd);
            size_t blockEnd = content.find('}', blockStart);
            if (blockStart == std::string::npos || blockEnd == std::string::npos) {
                pos = idEnd + 1;
                continue;
            }

            std::string gameBlock = content.substr(blockStart, blockEnd - blockStart + 1);

            // Parse moveListArray
            size_t movesPos = gameBlock.find("\"moveListArray\"");
            if (movesPos != std::string::npos) {
                size_t arrayStart = gameBlock.find('[', movesPos);
                size_t arrayEnd = gameBlock.find(']', arrayStart);
                if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
                    std::string movesArray = gameBlock.substr(arrayStart, arrayEnd - arrayStart + 1);
                    game.moves = parseMoves(movesArray);
                }
            }

            // Parse whiteMoveTimestampsArray
            size_t whitePos = gameBlock.find("\"whiteMoveTimestampsArray\"");
            if (whitePos != std::string::npos) {
                size_t arrayStart = gameBlock.find('[', whitePos);
                size_t arrayEnd = gameBlock.find(']', arrayStart);
                if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
                    std::string timestampsArray = gameBlock.substr(arrayStart, arrayEnd - arrayStart + 1);
                    game.whiteTimestamps = parseTimestamps(timestampsArray);
                }
            }

            // Parse blackMoveTimestampsArray
            size_t blackPos = gameBlock.find("\"blackMoveTimestampsArray\"");
            if (blackPos != std::string::npos) {
                size_t arrayStart = gameBlock.find('[', blackPos);
                size_t arrayEnd = gameBlock.find(']', arrayStart);
                if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
                    std::string timestampsArray = gameBlock.substr(arrayStart, arrayEnd - arrayStart + 1);
                    game.blackTimestamps = parseTimestamps(timestampsArray);
                }
            }

            if (!game.moves.empty()) {
                games.push_back(game);
            }

            pos = blockEnd + 1;
        }

        return games;
    }

    void analyzeGame(const GameData& game, const std::string depth, const std::string id) {
        auto& dbWrapper = Database::getInstance();
        auto coll = dbWrapper.getCollection("game_info_db", "labeled_position_data");
        std::vector<bsoncxx::document::value> docs;

        // Initialize chess position
        ChessPosition position;

        // Start from initial position
        std::string moveSeq = "position startpos moves";

        // Get initial position evaluation
        engine.sendCommand(moveSeq);
        double* result = engine.evaluate(depth);
        double evalBefore = 0;
        bool isCheckBefore = position.isInCheck(true); // White starts
        double legalMovesBefore = 20;
        double legalMovesAfter = 20;
        std::string fenBefore = "startup";
        std::string fenAfter = "";

        for (size_t i = 0; i < game.moves.size(); ++i) {
            // Determine whose move it is
            bool isWhiteMove = (i % 2 == 0);

            // Apply the move to our position tracker
            bool moveValid = position.makeMove(game.moves[i]);

            // Apply the move to Stockfish
            moveSeq += " " + game.moves[i];
            engine.sendCommand(moveSeq);
            fenAfter = engine.getFenPosition();
            
            // Get evaluation after the move
            double* result = engine.evaluate(depth, isWhiteMove);
            double evalAfter = result[0];
            legalMovesAfter = result[1];

            // Check if king is in check after the move
            bool isCheckAfter = false;
            if (moveValid) {
                // After the move, check if the opponent's king is in check
                isCheckAfter = position.isInCheck(!isWhiteMove);
            }

            // Get timestamp info
            double timeSpent = 0;
            int timeRemaining = 0;
            double timeSpentOnMoveBeforeIt = 0;

            if (isWhiteMove && i / 2 < game.whiteTimestamps.size()) {
                size_t whiteIndex = i / 2;

                if (whiteIndex == 0) {
                    timeSpent = 1800 - game.whiteTimestamps[whiteIndex + 1];
                }
                else {
                    timeSpent = game.whiteTimestamps[whiteIndex - 1] - game.whiteTimestamps[whiteIndex];
                }

                if (whiteIndex == 0) {
                    timeRemaining = game.whiteTimestamps[0];
                }
                else if (whiteIndex < game.whiteTimestamps.size()) {
                    timeRemaining = game.whiteTimestamps[whiteIndex];
                }

                if (whiteIndex < 1) {
                    timeSpentOnMoveBeforeIt = 0;
                }
                else {
                    timeSpentOnMoveBeforeIt = game.blackTimestamps[whiteIndex - 1] - game.blackTimestamps[whiteIndex];
                }
            }
            else if (!isWhiteMove && i / 2 < game.blackTimestamps.size()) {
                size_t blackIndex = i / 2;

                if (blackIndex == 0) {
                    timeSpent = 1800 - game.blackTimestamps[blackIndex];
                }
                else {
                    timeSpent = game.blackTimestamps[blackIndex - 1] - game.blackTimestamps[blackIndex];
                }

                if (blackIndex == 0) {
                    timeRemaining = game.blackTimestamps[0];
                }
                else if (blackIndex < game.blackTimestamps.size()) {
                    timeRemaining = game.blackTimestamps[blackIndex];
                }

                if (blackIndex < 1) {
                    timeSpentOnMoveBeforeIt = 0;
                }
                else {
                    timeSpentOnMoveBeforeIt = game.whiteTimestamps[blackIndex - 1] - game.whiteTimestamps[blackIndex];
                }
            }

            timeSpent = timeSpent * 0.1;
            timeSpentOnMoveBeforeIt = timeSpentOnMoveBeforeIt * 0.1;

            bool kingSideCastle = position.kingSideCastle();
            bool queenSideCastle = position.queenSideCastle();
            
            bsoncxx::builder::stream::document doc{};
            doc << "move" << static_cast<int32_t>(i + 1)
                << "kingSideCastle" << static_cast<int32_t>(kingSideCastle ? 1 : 0)
                << "queenSideCastle" << static_cast<int32_t>(queenSideCastle ? 1 : 0)
                << "isCheckBefore" << static_cast<int32_t>(isCheckBefore ? 1 : 0)
                << "isCheckAfter" << static_cast<int32_t>(isCheckAfter ? 1 : 0)
                << "evalDelta" << (evalAfter - evalBefore) // assuming this is double already
                << "evalBefore" << evalBefore
                << "evalAfter" << evalAfter
                << "timeRemaining" << (timeRemaining * 0.1) // keep as double
                << "timeSpentOnMoveBeforeIt" << (timeSpentOnMoveBeforeIt * 0.1) // keep as double
                << "legalMovesBefore" << static_cast<int32_t>(legalMovesBefore)
                << "legalMovesAfter" << static_cast<int32_t>(legalMovesAfter)
                << "timeSpent" << static_cast<int32_t>(timeSpent)
                << "fenBefore" << fenBefore
                << "fenAfter" << fenAfter
                << "requestID" << id;


            docs.push_back(doc << bsoncxx::builder::stream::finalize);
            // Update for next iteration
            evalBefore = evalAfter;
            isCheckBefore = isCheckAfter;
            legalMovesBefore = legalMovesAfter;
            fenBefore = fenAfter;

            // Clean up memory
            delete[] result;
        }

        // Insert them all at once
        try {
            if (!docs.empty()) {
                auto resultk = coll.insert_many(docs);
                std::cout << "Successfully inserted " << docs.size() << " positions for game: " << game.gameId << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error inserting positions into MongoDB: " << e.what() << std::endl;
        }
    }
};

// Helper function to extract a vector of integers from a BSON array of strings [CORRECTED]
void extractIntVector(const bsoncxx::document::view& doc_view, const std::string& key, std::vector<int>& target_vector) {
    auto field = doc_view[key];
    if (field && field.type() == bsoncxx::type::k_array) {
        for (const auto& element : field.get_array().value) {
            // Use k_string and construct std::string directly
            if (element.type() == bsoncxx::type::k_string) { 
                try {
                    // Create std::string from the string_view-like object
                    std::string str_val(element.get_string().value);
                    target_vector.push_back(std::stoi(str_val));
                } catch (const std::exception& e) {
                    std::cerr << "Warning: Could not convert timestamp string to int: " << e.what() << std::endl;
                }
            }
        }
    }
}

// Helper function to extract a vector of strings from a BSON array of strings [CORRECTED]
void extractStringVector(const bsoncxx::document::view& doc_view, const std::string& key, std::vector<std::string>& target_vector) {
    auto field = doc_view[key];
    if (field && field.type() == bsoncxx::type::k_array) {
        for (const auto& element : field.get_array().value) {
            // Use k_string and construct std::string directly
            if (element.type() == bsoncxx::type::k_string) {
                target_vector.push_back(std::string(element.get_string().value));
            }
        }
    }
}



// Simulate a long-running task
void longTask(std::string id) {
    GameAnalyzer analyzer;

    if (!analyzer.init("./stockfish")) {
        std::cout << "Error: Stockfish not found. Make sure stockfish is available." << std::endl;
    } else {
        std::cout << "Stockfish is ready!." << std::endl;

        auto& dbWrapper = Database::getInstance();
        auto coll = dbWrapper.getCollection("game_info_db", "chess_smart_thinking_data");

        auto filter = bsoncxx::builder::stream::document{} << "requestID" << id << bsoncxx::builder::stream::finalize;
        
        // Execute the query and get a cursor for the results
        mongocxx::cursor cursor = coll.find(filter.view());

        // Iterate over each document found by the cursor
        for (bsoncxx::document::view doc : cursor) {

            std::cout << "Analyzing game: " << std::endl;

            GameData game;


            // 1. Extract the gameId from the _id field
            if (doc["_id"] && doc["_id"].type() == bsoncxx::type::k_oid) {
                game.gameId = doc["_id"].get_oid().value.to_string();
                
                // 2. Extract the moves array
                extractStringVector(doc, "moveListArray", game.moves);

                // 3. Extract timestamp arrays (and convert string elements to int)
                extractIntVector(doc, "whiteMoveTimestampsArray", game.whiteTimestamps);
                extractIntVector(doc, "blackMoveTimestampsArray", game.blackTimestamps);

                std::cout << "Analyzing game: " << game.gameId << std::endl;
                std::cout << "Moves: " << game.moves.size() << std::endl;
                // 4. Call your analysis function with the populated struct
                analyzer.analyzeGame(game, std::string(doc["Analyzation_depth"].get_string().value), id);
            } else {
                std::cout << "error" << std::endl;
            }
        }
    }

    if (callArbiter(id)) {
        std::cout << "Successfully called arbiter-api!" << std::endl;
    } else {
        std::cerr << "Failed to call arbiter-api after retries." << std::endl;
    }
}

class TaskQueue {
public:
    TaskQueue() : stop(false) {
        worker = std::thread([this] { this->process(); });
    }

    ~TaskQueue() {
        {
            std::unique_lock<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();
        worker.join();
    }

    void addTask(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            tasks.push(std::move(task));
        }
        cv.notify_one();
    }

private:
    void process() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]{ return !tasks.empty() || stop; });
                if (stop && tasks.empty()) return;
                task = std::move(tasks.front());
                tasks.pop();
            }
            task(); // run the task outside the lock
        }
    }

    std::thread worker;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop;
};


// Global queue for background tasks
TaskQueue taskQueue;

// HTTP handler (pseudo)
int handleRequest(std::string jobId) {
    taskQueue.addTask([jobId] { longTask(jobId); });
    return 202; // HTTP 202 Accepted
}

class OkAPI {
public:
    OkAPI(const std::string& address) 
        : m_listener(address) {
        // Register endpoints
        m_listener.support(methods::GET, std::bind(&OkAPI::handle_get, this, std::placeholders::_1));
        m_listener.support(methods::POST, std::bind(&OkAPI::handle_post, this, std::placeholders::_1));
        m_listener.support(methods::OPTIONS, std::bind(&OkAPI::handle_options, this, std::placeholders::_1));
    }

    void start() {
        m_listener.open().then([this]() {
            std::cout << "Listening on: " 
                      << utility::conversions::to_utf8string(m_listener.uri().to_string()) 
                      << std::endl;
        }).wait();
    }

private:
    http_listener m_listener;

    void add_cors_headers(http_response& response) {
        response.headers().add(U("Access-Control-Allow-Origin"), U("*"));
        response.headers().add(U("Access-Control-Allow-Methods"), U("GET, POST, OPTIONS"));
        response.headers().add(U("Access-Control-Allow-Headers"), U("Content-Type, Accept"));
    }

    void handle_get(http_request request) {
        auto path = uri::split_path(uri::decode(request.relative_uri().path()));
        std::cout << "GET request received at path: ";
        for (auto &segment : path) std::cout << "/" << utility::conversions::to_utf8string(segment);
        std::cout << std::endl;

        web::json::value response;

        
        
        if (path.size() == 2 && path[0] == U("analyze") && path[1] != "") {
            // Capture the {id} from the URL
            utility::string_t id = path[1];

            std::cout << id << std::endl;
            std::string* ptr = &id;
            
            std::cout << "Status: " << handleRequest(id) << std::endl;
            request.reply(status_codes::OK, U("Analyzing data with id: ") + id);
            
        } else if (path.size() == 1 && path[0] == U("health")) {
            response[U("message")] = web::json::value::string(U("I am healthy"));
            request.reply(status_codes::OK, response);
        } else {
            response[U("message")] = web::json::value::string(U("Not found"));
            request.reply(404, response);
        }
    }

    void handle_post(http_request request) {
        auto path = uri::split_path(uri::decode(request.relative_uri().path()));
        std::cout << "POST request received at path: ";
        for (auto &segment : path) std::cout << "/" << utility::conversions::to_utf8string(segment);
        std::cout << std::endl;

        if (path.size() == 1 && path[0] == U("echo")) {
            request.extract_json()
            .then([request](pplx::task<web::json::value> task) {
                try {
                    auto body = task.get();
                    // Echo back whatever JSON was sent
                    request.reply(status_codes::OK, body);
                } catch (std::exception& e) {
                    web::json::value error;
                    error[U("error")] = web::json::value::string(utility::conversions::to_string_t(e.what()));
                    request.reply(status_codes::BadRequest, error);
                }
            });
        } else {
            web::json::value response;
            response[U("error")] = web::json::value::string(U("Unknown POST endpoint"));
            request.reply(status_codes::NotFound, response);
        }
    }

    void handle_options(http_request request) {
        http_response res(status_codes::OK);
        add_cors_headers(res);
        request.reply(res);
    }

};






int main() {
    OkAPI api("http://0.0.0.0:8080");
    api.start();

    std::cout << "Server running... waiting for SIGTERM or SIGINT\n";
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
