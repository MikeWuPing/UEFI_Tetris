#include "types.h"

// Forward declarations
VOID InitGame(GameState *game);
VOID SpawnNewPiece(GameState *game);
INT8 (*GetCurrentShape(GameState *game))[4];
INT8 (*GetShapeForPiece(INT8 piece, INT8 rotation))[4];
BOOLEAN IsValidPosition(GameState *game, INT8 offsetX, INT8 offsetY);
VOID LockPieceToBoard(GameState *game);
VOID ClearLines(GameState *game);
BOOLEAN MovePiece(GameState *game, INT8 dx, INT8 dy);
VOID RotatePiece(GameState *game);
BOOLEAN SoftDrop(GameState *game);
VOID HardDrop(GameState *game);
VOID HoldPiece(GameState *game);

// Tetromino shape definitions - 7 types, 4 rotation states
// Using SRS-style rotation with consistent pivot point
// All shapes positioned so the rotation pivot is at center of 4x4 grid

// I-piece: 4 blocks in a line, pivot at center
static INT8 I_SHAPE[4][4][4] = {
  {{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}},  // spawn (horizontal)
  {{0,0,1,0}, {0,0,1,0}, {0,0,1,0}, {0,0,1,0}},  // R (vertical)
  {{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}},  // 2 (horizontal)
  {{0,0,1,0}, {0,0,1,0}, {0,0,1,0}, {0,0,1,0}}   // L (vertical)
};

// O-piece: 2x2 square, no rotation effect
static INT8 O_SHAPE[4][4][4] = {
  {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
  {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
  {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
  {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}}
};

// T-piece: pivot at the center block (1,1)
// All rotations keep the center block at (1,1)
static INT8 T_SHAPE[4][4][4] = {
  {{0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},  // spawn (T up)
  {{0,1,0,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0}},  // R (T right)
  {{0,0,0,0}, {1,1,1,0}, {0,1,0,0}, {0,0,0,0}},  // 2 (T down)
  {{0,1,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0}}   // L (T left)
};

// S-piece: zigzag, pivot at the center where blocks meet
// Horizontal: blocks at row 0 (cols 1,2) and row 1 (cols 0,1)
// Vertical: blocks at col 1 (rows 0,1) and col 2 (rows 1,2)
static INT8 S_SHAPE[4][4][4] = {
  {{0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},  // spawn (horizontal)
  {{0,1,0,0}, {0,1,1,0}, {0,0,1,0}, {0,0,0,0}},  // R (vertical)
  {{0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},  // 2 (horizontal)
  {{0,1,0,0}, {0,1,1,0}, {0,0,1,0}, {0,0,0,0}}   // L (vertical)
};

// Z-piece: opposite zigzag
// Horizontal: blocks at row 0 (cols 0,1) and row 1 (cols 1,2)
// Vertical: blocks at col 1 (rows 1,2) and col 2 (rows 0,1)
static INT8 Z_SHAPE[4][4][4] = {
  {{1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},  // spawn (horizontal)
  {{0,0,1,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0}},  // R (vertical)
  {{1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},  // 2 (horizontal)
  {{0,0,1,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0}}   // L (vertical)
};

// J-piece: L-shape pointing left/up/right/down
// Pivot at the center block of the 3-block line
static INT8 J_SHAPE[4][4][4] = {
  {{1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},  // spawn (J left)
  {{0,1,1,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0}},  // R (J up)
  {{0,0,0,0}, {1,1,1,0}, {0,0,1,0}, {0,0,0,0}},  // 2 (J right)
  {{0,1,0,0}, {0,1,0,0}, {1,1,0,0}, {0,0,0,0}}   // L (J down)
};

// L-piece: opposite L-shape
static INT8 L_SHAPE[4][4][4] = {
  {{0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},  // spawn (L right)
  {{0,1,0,0}, {0,1,0,0}, {0,1,1,0}, {0,0,0,0}},  // R (L up)
  {{0,0,0,0}, {1,1,1,0}, {1,0,0,0}, {0,0,0,0}},  // 2 (L left)
  {{1,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0}}   // L (L down)
};

// Tetromino colors - brighter, more vibrant colors
RGB g_tetrominoColors[TETROMINO_COUNT] = {
  {0, 240, 240, 0},    // I - Cyan
  {240, 240, 0, 0},    // O - Yellow
  {160, 0, 240, 0},    // T - Purple
  {0, 240, 0, 0},      // S - Green
  {240, 0, 0, 0},      // Z - Red
  {0, 0, 240, 0},      // J - Blue
  {240, 160, 0, 0}     // L - Orange
};

// Tetromino shape table
static INT8 (*g_shapes[TETROMINO_COUNT])[4][4] = {
  (INT8(*)[4][4])I_SHAPE,
  (INT8(*)[4][4])O_SHAPE,
  (INT8(*)[4][4])T_SHAPE,
  (INT8(*)[4][4])S_SHAPE,
  (INT8(*)[4][4])Z_SHAPE,
  (INT8(*)[4][4])J_SHAPE,
  (INT8(*)[4][4])L_SHAPE
};

// Random number generator state
static UINT32 g_randomSeed = 12345;

// Simple random number generator
static UINT32 my_rand(VOID) {
  g_randomSeed = g_randomSeed * 1103515245 + 12345;
  return (g_randomSeed >> 16) & 0x7FFF;
}

// Initialize game
VOID InitGame(GameState *game) {
  INT32 i, j;

  // Clear game board
  for (i = 0; i < BOARD_HEIGHT; i++) {
    for (j = 0; j < BOARD_WIDTH; j++) {
      game->board[i][j] = 0;
    }
  }

  // Initialize game state
  game->score = 0;
  game->lines = 0;
  game->level = 1;
  game->gameOver = FALSE;
  game->paused = FALSE;
  game->started = FALSE;
  game->currentPiece = -1;
  game->currentRotation = 0;
  game->pieceX = 0;
  game->pieceY = 0;
  game->nextPiece = -1;
  game->holdPiece = -1;
  game->holdUsed = FALSE;
}

// Spawn new piece (use next piece if available)
VOID SpawnNewPiece(GameState *game) {
  // Use next piece if available, otherwise generate new
  if (game->nextPiece >= 0) {
    game->currentPiece = game->nextPiece;
  } else {
    game->currentPiece = my_rand() % TETROMINO_COUNT;
  }

  // Generate next piece
  game->nextPiece = my_rand() % TETROMINO_COUNT;

  game->currentRotation = 0;
  game->pieceX = BOARD_WIDTH / 2 - 2;
  game->pieceY = 0;
  game->holdUsed = FALSE;  // Reset hold usage for new piece

  // Check game over
  if (!IsValidPosition(game, 0, 0)) {
    game->gameOver = TRUE;
  }
}

// Get current piece shape
INT8 (*GetCurrentShape(GameState *game))[4] {
  // g_shapes[piece] points to rotation 0 of that piece
  // g_shapes[piece][rotation] gives the rotation state (a INT8[4][4])
  // which decays to INT8(*)[4] pointing to the first row
  return g_shapes[game->currentPiece][game->currentRotation];
}

// Get shape for any piece (used for preview and hold display)
INT8 (*GetShapeForPiece(INT8 piece, INT8 rotation))[4] {
  if (piece < 0 || piece >= TETROMINO_COUNT) {
    return NULL;
  }
  return g_shapes[piece][rotation];
}

// Check if position is valid
BOOLEAN IsValidPosition(GameState *game, INT8 offsetX, INT8 offsetY) {
  INT8 (*shape)[4] = GetCurrentShape(game);
  INT8 i, j;
  INT8 newX, newY;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      if (shape[i][j]) {
        newX = game->pieceX + offsetX + j;
        newY = game->pieceY + offsetY + i;

        // Check boundaries
        if (newX < 0 || newX >= BOARD_WIDTH || newY >= BOARD_HEIGHT) {
          return FALSE;
        }

        // Check collision with locked pieces
        if (newY >= 0 && game->board[newY][newX] != 0) {
          return FALSE;
        }
      }
    }
  }

  return TRUE;
}

// Lock piece to board
VOID LockPieceToBoard(GameState *game) {
  INT8 (*shape)[4] = GetCurrentShape(game);
  INT8 i, j;
  INT8 boardX, boardY;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      if (shape[i][j]) {
        boardX = game->pieceX + j;
        boardY = game->pieceY + i;

        if (boardY >= 0 && boardY < BOARD_HEIGHT && boardX >= 0 && boardX < BOARD_WIDTH) {
          game->board[boardY][boardX] = game->currentPiece + 1;
        }
      }
    }
  }
}

// Clear full lines
VOID ClearLines(GameState *game) {
  INT8 i, j, k;
  INT8 cleared = 0;

  for (i = BOARD_HEIGHT - 1; i >= 0; i--) {
    // Check if line is full
    for (j = 0; j < BOARD_WIDTH; j++) {
      if (game->board[i][j] == 0) {
        break;
      }
    }

    if (j == BOARD_WIDTH) {
      // Full line, delete and shift up
      for (k = i; k > 0; k--) {
        for (j = 0; j < BOARD_WIDTH; j++) {
          game->board[k][j] = game->board[k - 1][j];
        }
      }
      // Clear top line
      for (j = 0; j < BOARD_WIDTH; j++) {
        game->board[0][j] = 0;
      }
      cleared++;
      i++;  // Re-check this line
    }
  }

  // Update score
  if (cleared > 0) {
    // 1 line = 100, 2 lines = 300, 3 lines = 500, 4 lines = 800
    static UINT32 lineScores[] = {0, 100, 300, 500, 800};
    game->score += lineScores[cleared] * game->level;
    game->lines += cleared;

    // Level up every 10 lines
    game->level = game->lines / 10 + 1;
  }
}

// Move piece
BOOLEAN MovePiece(GameState *game, INT8 dx, INT8 dy) {
  if (IsValidPosition(game, dx, dy)) {
    game->pieceX += dx;
    game->pieceY += dy;
    return TRUE;
  }
  return FALSE;
}

// Rotate piece with wall kick tests
VOID RotatePiece(GameState *game) {
  INT8 newRotation = (game->currentRotation + 1) % 4;
  INT8 oldRotation = game->currentRotation;

  game->currentRotation = newRotation;

  // Wall kick test offsets: {dx, dy}
  // Try multiple positions to find a valid rotation spot
  static INT8 kickTests[8][2] = {
    { 0,  0},  // Original position
    {-1,  0},  // Left 1
    { 1,  0},  // Right 1
    {-2,  0},  // Left 2 (for I piece near edge)
    { 2,  0},  // Right 2 (for I piece near edge)
    { 0, -1},  // Up 1
    {-1, -1},  // Up-left
    { 1, -1}   // Up-right
  };

  INT8 i;
  for (i = 0; i < 8; i++) {
    if (IsValidPosition(game, kickTests[i][0], kickTests[i][1])) {
      game->pieceX += kickTests[i][0];
      game->pieceY += kickTests[i][1];
      return;
    }
  }

  // Cannot rotate, revert
  game->currentRotation = oldRotation;
}

// Soft drop (one step)
BOOLEAN SoftDrop(GameState *game) {
  if (IsValidPosition(game, 0, 1)) {
    game->pieceY++;
    return TRUE;
  } else {
    // Cannot drop, lock piece
    LockPieceToBoard(game);
    ClearLines(game);
    SpawnNewPiece(game);
    return FALSE;
  }
}

// Hard drop (to bottom)
VOID HardDrop(GameState *game) {
  while (IsValidPosition(game, 0, 1)) {
    game->pieceY++;
    game->score += 2;  // Bonus points for hard drop
  }
  LockPieceToBoard(game);
  ClearLines(game);
  SpawnNewPiece(game);
}

// Hold piece function
VOID HoldPiece(GameState *game) {
  // Can only hold once per piece
  if (game->holdUsed) {
    return;
  }

  if (game->holdPiece < 0) {
    // No piece held yet, store current and spawn new
    game->holdPiece = game->currentPiece;
    SpawnNewPiece(game);
  } else {
    // Swap current and held piece
    INT8 temp = game->currentPiece;
    game->currentPiece = game->holdPiece;
    game->holdPiece = temp;

    // Reset position
    game->currentRotation = 0;
    game->pieceX = BOARD_WIDTH / 2 - 2;
    game->pieceY = 0;

    // Check if swapped piece is valid
    if (!IsValidPosition(game, 0, 0)) {
      // Swap back
      temp = game->currentPiece;
      game->currentPiece = game->holdPiece;
      game->holdPiece = temp;
      return;
    }
  }

  game->holdUsed = TRUE;
}