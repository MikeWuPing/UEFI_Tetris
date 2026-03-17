#ifndef TYPES_H
#define TYPES_H

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextIn.h>

// Game area constants
#define BOARD_WIDTH      10
#define BOARD_HEIGHT     20
#define BLOCK_SIZE       28    // Block size in pixels (increased from 20)

// Game area position (centered in 1024x768)
#define GAME_AREA_X      120
#define GAME_AREA_Y      80

// Info area (right side)
#define INFO_AREA_X      450
#define INFO_AREA_Y      80
#define INFO_AREA_WIDTH  280
#define INFO_AREA_HEIGHT 560

// Next piece preview area
#define PREVIEW_X        INFO_AREA_X + 20
#define PREVIEW_Y        INFO_AREA_Y + 120
#define PREVIEW_SIZE     (BLOCK_SIZE * 4)

// Hold piece area
#define HOLD_X           INFO_AREA_X + 160
#define HOLD_Y           INFO_AREA_Y + 120

// Tetromino types (I, O, T, S, Z, J, L)
#define TETROMINO_COUNT 7

// Color definition
typedef struct {
  UINT8  Red;
  UINT8  Green;
  UINT8  Blue;
  UINT8  Reserved;
} RGB;

// Tetromino structure
typedef struct {
  INT8   shape[4][4];    // 4x4 rotation state
  RGB    color;          // Color
} Tetromino;

// Game state
typedef struct {
  INT8   board[BOARD_HEIGHT][BOARD_WIDTH];  // Game board, 0 = empty, other = tetromino type + 1
  INT8   currentPiece;        // Current piece type (0-6)
  INT8   currentRotation;     // Current rotation state (0-3)
  INT8   pieceX;              // Current piece X position
  INT8   pieceY;              // Current piece Y position
  INT8   nextPiece;           // Next piece type (0-6)
  INT8   holdPiece;           // Held piece type (-1 = none)
  BOOLEAN holdUsed;           // Whether hold was used this turn
  UINT32 score;               // Score
  UINT32 lines;               // Lines cleared
  UINT32 level;               // Level
  BOOLEAN gameOver;           // Game over flag
  BOOLEAN paused;             // Paused flag
  BOOLEAN started;            // Game started flag
} GameState;

// Render color table
extern RGB g_tetrominoColors[TETROMINO_COUNT];

#endif // TYPES_H