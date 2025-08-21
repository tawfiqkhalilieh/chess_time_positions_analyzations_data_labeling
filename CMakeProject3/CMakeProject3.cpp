#include <iostream>
#include <string>
#include <windows.h>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <vector>
#include <sstream>
#include <array>
#include <chrono>

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
    HANDLE hChildStdinWr = nullptr;
    HANDLE hChildStdoutRd = nullptr;
    PROCESS_INFORMATION piProcInfo;
    bool engineRunning = false;

public:
    ~StockfishEngine() {
        if (engineRunning) {
            sendCommand("quit");
            if (piProcInfo.hProcess) {
                WaitForSingleObject(piProcInfo.hProcess, 1000);
                CloseHandle(piProcInfo.hProcess);
                CloseHandle(piProcInfo.hThread);
            }
            if (hChildStdinWr) CloseHandle(hChildStdinWr);
            if (hChildStdoutRd) CloseHandle(hChildStdoutRd);
        }
    }

    bool init(const std::string& path = "stockfish.exe") {
        SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
        HANDLE hChildStdinRd, hChildStdoutWr;

        if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0) ||
            !SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0) ||
            !CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0) ||
            !SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0)) {
            return false;
        }

        STARTUPINFO si = { sizeof(STARTUPINFO) };
        si.hStdError = si.hStdOutput = hChildStdoutWr;
        si.hStdInput = hChildStdinRd;
        si.dwFlags = STARTF_USESTDHANDLES;

        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
        std::string cmd = path;

        if (!CreateProcess(nullptr, const_cast<char*>(cmd.c_str()), nullptr, nullptr,
            TRUE, 0, nullptr, nullptr, &si, &piProcInfo)) {
            CloseHandle(hChildStdoutRd); CloseHandle(hChildStdoutWr);
            CloseHandle(hChildStdinRd); CloseHandle(hChildStdinWr);
            return false;
        }

        CloseHandle(hChildStdoutWr); CloseHandle(hChildStdinRd);
        engineRunning = true;

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
        DWORD written;
        WriteFile(hChildStdinWr, command.c_str(), command.length(), &written, nullptr);
    }

    std::string readLine() {
        std::string line;
        char ch;
        DWORD read;

        while (true) {
            if (!ReadFile(hChildStdoutRd, &ch, 1, &read, nullptr) || read == 0) {
                Sleep(1);
                continue;
            }

            if (ch == '\n') {
                break;
            }
            else if (ch != '\r') {
                line += ch;
            }
        }

        return line;
    }


    double* evaluate(bool isWhiteToMove = true) {
        sendCommand("go perft 1");

        /*
            In the current version of stockfish, excuting the command {go perft 1}, outputs all the legal moves in the position.
            The following chunk of code is utilizing the formate of the stockfish output to count the legal moves

            e.g:

            go perft 1
            info string Available processors: 0-7
            info string Using 1 thread
            info string NNUE evaluation using nn-1c0000000000.nnue (133MiB, (22528, 3072, 15, 32, 1))
            info string NNUE evaluation using nn-37f18f62d772.nnue (6MiB, (22528, 128, 15, 32, 1))
            a2a3: 1
            b2b3: 1
            d2d3: 1
            ...

            Nodes searched: 30


            ":" appears in the text by 1 + the number of moves.
        */
        
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

        arr[1] = count-1;

        // NOTE: The vision was to go to depth 20-22 ( which are commonly used in game reviews ) but for the lack of the power of calculation and for the main purpose of the project ( to be shipped fast ), unfourtunately, I had to limit the depth to 10. 
        sendCommand("go depth 10");
        /*
        In this section, I'm using a similar trick to the one I used in the previous section.

        Stockfish 17.1 by the Stockfish developers (see AUTHORS file)
        position set 1N6/p2k1pp1/7p/2p5/3b1P2/8/P5PP/2R2K2 b - - 0 30
        go depth 25
        info string Available processors: 0-11
        info string Using 1 thread
        info string NNUE evaluation using nn-1c0000000000.nnue (133MiB, (22528, 3072, 15, 32, 1))
        info string NNUE evaluation using nn-37f18f62d772.nnue (6MiB, (22528, 128, 15, 32, 1))
        info depth 1 seldepth 2 multipv 1 score cp 17 nodes 20 nps 6666 hashfull 0 tbhits 0 time 3 pv e2e4
        info depth 2 seldepth 3 multipv 1 score cp 34 nodes 45 nps 11250 hashfull 0 tbhits 0 time 4 pv e2e4
        info depth 3 seldepth 4 multipv 1 score cp 42 nodes 72 nps 18000 hashfull 0 tbhits 0 time 4 pv e2e4
        info depth 4 seldepth 7 multipv 1 score cp 39 nodes 512 nps 102400 hashfull 0 tbhits 0 time 5 pv g1f3 d7d5 d2d4
        info depth 5 seldepth 7 multipv 1 score cp 58 nodes 609 nps 121800 hashfull 0 tbhits 0 time 5 pv e2e4
        info depth 6 seldepth 8 multipv 1 score cp 57 nodes 752 nps 125333 hashfull 0 tbhits 0 time 6 pv e2e4 d7d5 e4d5 d8d5 g1f3
        info depth 7 seldepth 10 multipv 1 score cp 46 nodes 2019 nps 252375 hashfull 0 tbhits 0 time 8 pv e2e4 c7c5 d2d4 c5d4 g1f3
        info depth 8 seldepth 13 multipv 1 score cp 47 nodes 3436 nps 343600 hashfull 1 tbhits 0 time 10 pv e2e4 c7c5 b1c3 b8c6 g1f3 g8f6
        info depth 9 seldepth 13 multipv 1 score cp 94 nodes 4748 nps 395666 hashfull 1 tbhits 0 time 12 pv e2e4 c7c5 g1f3 b8c6 b1c3 b7b6
        info depth 10 seldepth 17 multipv 1 score cp 40 nodes 8620 nps 507058 hashfull 2 tbhits 0 time 17 pv e2e4 c7c5 g1f3 b8c6 b1c3 g8f6 d2d4 c5d4 f3d4 e7e6 d4c6 b7c6
        info depth 11 seldepth 17 multipv 1 score cp 41 nodes 15172 nps 561925 hashfull 4 tbhits 0 time 27 pv e2e4 c7c5 g1f3 e7e6 b1c3 b8c6 d2d4 c5d4 f3d4 g8f6 a2a3 f8e7 d4c6 d7c6
        info depth 12 seldepth 19 multipv 1 score cp 33 nodes 34187 nps 633092 hashfull 12 tbhits 0 time 54 pv e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4 g8f6 d4c6 d7c6 d1d8 e8d8 f1d3 f8b4 c2c3
        info depth 13 seldepth 17 multipv 1 score cp 35 nodes 37544 nps 625733 hashfull 12 tbhits 0 time 60 pv e2e4 e7e5 b1c3 b8c6 g1f3 f8b4 d2d4 e5d4 f3d4 g8f6 d4c6 d7c6 d1d8 e8d8
        info depth 14 seldepth 19 multipv 1 score cp 40 nodes 50495 nps 631187 hashfull 17 tbhits 0 time 80 pv e2e4 e7e5 g1f3 g8f6 b1c3 d7d6 d2d4 e5d4 d1d4 f8e7
        info depth 15 seldepth 18 multipv 1 score cp 39 nodes 64450 nps 625728 hashfull 21 tbhits 0 time 103 pv e2e4 e7e5 g1f3 g8f6 d2d4 f6e4 d4e5 d7d5 f1d3 b8c6 e1g1 c8e6 b1c3 e4c3 b2c3
        info depth 16 seldepth 20 multipv 1 score cp 37 nodes 80751 nps 616419 hashfull 27 tbhits 0 time 131 pv e2e4 e7e5 g1f3 g8f6 d2d4 f6e4 f3e5 d7d5 b1d2 b8d7 d2e4 d5e4 d1h5 g7g6 h5e2 d7e5 e2e4 f8g7 d4e5
        info depth 17 seldepth 23 multipv 1 score cp 37 nodes 100673 nps 621438 hashfull 34 tbhits 0 time 162 pv e2e4 e7e5 g1f3 g8f6 b1c3 d7d6 d2d4 e5d4 f3d4 f8e7 f1b5 c7c6 b5d3 e8g8 e1g1 c6c5 d4f3 b8c6
        info depth 18 seldepth 28 multipv 1 score cp 30 nodes 242639 nps 636847 hashfull 94 tbhits 0 time 381 pv e2e4 e7e5 g1f3 b8c6 f1e2 d7d5 e4d5 d8d5 d2d3 c8e6 e1g1 f8d6 f1e1 h7h6
        info depth 19 seldepth 27 multipv 1 score cp 40 nodes 294310 nps 642598 hashfull 109 tbhits 0 time 458 pv e2e4 e7e5 g1f3 g8f6 f3e5 d7d6 e5f3 f6e4 d2d4 d6d5 f1d3 f8b4 c2c3 b4d6 e1g1 e8g8 c3c4 c7c6 d1c2 b8a6 d3e4 d5e4 c2e4 f8e8
        info depth 20 seldepth 33 multipv 1 score cp 40 nodes 356864 nps 652402 hashfull 127 tbhits 0 time 547 pv e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f6e4 d2d4 f8e7 f1e1 b7b5 e1e4 b5a4 d4e5
        info depth 21 seldepth 35 multipv 1 score cp 40 nodes 693162 nps 638858 hashfull 257 tbhits 0 time 1085 pv e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4 g8f6 d4c6 d7c6 d1d8 e8d8 b1c3 d8e8 c1d2 a7a5 a2a3 f8d6 f2f4
        info depth 22 seldepth 29 multipv 1 score cp 38 nodes 753875 nps 638336 hashfull 279 tbhits 0 time 1181 pv e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 e8g8 c2c3 c6a5 b3c2 d7d5 d2d4 e5d4 e4d5 f8e8 c3d4 c8b7 b1c3 f6d5
        info depth 23 seldepth 31 multipv 1 score cp 34 nodes 901681 nps 644057 hashfull 329 tbhits 0 time 1400 pv e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 d2d3 b7b5 a4b3 d7d6 a2a4 c6a5 a4b5 a5b3 c2b3 e8g8 h2h3 c8b7
        info depth 24 seldepth 32 multipv 1 score cp 36 nodes 1179519 nps 649514 hashfull 426 tbhits 0 time 1816 pv e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f6e4 d2d4 b7b5 a4b3 d7d5 d4e5 c8e6 b1d2 e4c5 c2c3 c5b3 d2b3 f8e7 f3d4 c6d4 b3d4
        info depth 25 seldepth 35 multipv 1 score cp 24 nodes 2042808 nps 635596 hashfull 668 tbhits 0 time 3214 pv e2e4 e7e5 g1f3 b8c6 f1b5 g8f6 e1g1 f6e4 f1e1 e4d6 f3e5 f8e7 b5f1 d6f5 e5f3 c6d4 c2c3 d4f3 d1f3 d7d6 d2d4 e8g8 f1d3 f5h4 f3e2 f8e8 g2g3 h4g6 h2h4 c7c6 h4h5 g6f8 h5h6 g7g6
        bestmove e2e4 ponder e7e5 <<- Look at the line above it!
        */
        
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
                // std::cout << "Stockfish: " << line << std::endl;
                break;
            }
        }

        // --- Print the final evaluation ---
        /*


        NOTE: might be used later - this code divides the evaluation score by 100 to convert centipawns to pawns ( the metric used by chess.com ) 
        if (mateFound) {
            arr[0] = 25;
        }
        else {
            // Convert centipawns to a more human-readable pawn value.
            double pawnValue = evalScore / 100.0;
            if (pawnValue > 25) {
                pawnValue = 25;
            }
            
            // return pawnValue;
            arr[0] = pawnValue;
        }
        */
        

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
                // std::cout << fenLine << std::endl;
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
    bool init() {
        return engine.init();
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

    void analyzeGame(const GameData& game) {
        std::cout << "\n=== Analyzing Game: " << game.gameId << " ===" << std::endl;
        std::cout << "Total moves: " << game.moves.size() << std::endl;
        std::cout << std::string(80, '=') << std::endl;


        // Initialize chess position
        ChessPosition position;

        // Start from initial position
        std::string moveSeq = "position startpos moves";

        // Get initial position evaluation
        engine.sendCommand(moveSeq);
        double* result = engine.evaluate();
        double evalBefore = 0;
        bool isCheckBefore = position.isInCheck(true); // White starts
        double legalMovesBefore = 20;
        double legalMovesAfter = 20;
        std::string fenBefore = "startup";
        std::string fenAfter = "";


        for (size_t i = 0; i < game.moves.size(); ++i) {


            // auto start_time = std::chrono::high_resolution_clock::now();
            
            // Determine whose move it is
            bool isWhiteMove = (i % 2 == 0);

            

            // Apply the move to our position tracker
            bool moveValid = position.makeMove(game.moves[i]);

            // Apply the move to Stockfish
            moveSeq += " " + game.moves[i];
            engine.sendCommand(moveSeq);
            fenAfter = engine.getFenPosition();
            

            // Get evaluation after the move
            double* result = engine.evaluate(isWhiteMove);
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

             std::ofstream file("analyzed_game_information.csv", std::ios::app); // Open in append mode
             if (!file) {
                 std::cerr << "Error: Could not open file " << "analyzed_game_information.csv" << std::endl;
                 return;
             }

             file << i+1 << "," << (kingSideCastle ? 1 : 0) << "," << (queenSideCastle ? 1 : 0) << "," << (isCheckBefore ? 1 : 0) << "," << (isCheckAfter ? 1 : 0) << "," << evalAfter - evalBefore << "," << evalBefore << "," << evalAfter << "," << timeRemaining * 0.1 << "," << timeSpentOnMoveBeforeIt * 0.1 << "," << legalMovesBefore << "," << legalMovesAfter << "," << timeSpent << "," << fenBefore << "," << fenAfter << "," << "\n";
             file.close();

            // Update for next iteration
            evalBefore = evalAfter;
            evalAfter = 0.0;
            isCheckBefore = isCheckAfter;
            fenBefore = fenAfter;
            legalMovesBefore = legalMovesAfter;
            legalMovesAfter = 20;
        }

        std::cout << std::string(80, '=') << std::endl;
    }
};

int main() {
    GameAnalyzer analyzer;

    std::cout << "Chess Game Analyzer with Check Detection (Depth 11)" << std::endl;
    std::cout << "=================================================" << std::endl;

    if (!analyzer.init()) {
        std::cout << "Error: Stockfish not found. Make sure stockfish.exe is available." << std::endl;
        return 1;
    }

    std::cout << "Stockfish ready!" << std::endl;

    for (int i = 0; i < 78; i++) {
        // Parse games from JSON file
        auto games = analyzer.parseGameFile("game_information" + std::to_string(i) + ".json");

        if (games.empty()) {
            std::cout << "No games found in game_information" + std::to_string(i) + ".json" << std::endl;
            return 1;
        }

        std::cout << "Found " << games.size() << " games" << std::endl;

        // Analyze each game
        for (const auto& game : games) {
            analyzer.analyzeGame(game);
        }

    }
    return 0;
}