#include "types.h"

// Forward declarations
extern VOID InitGame(GameState *game);
extern VOID SpawnNewPiece(GameState *game);
extern BOOLEAN MovePiece(GameState *game, INT8 dx, INT8 dy);
extern BOOLEAN SoftDrop(GameState *game);
extern VOID HardDrop(GameState *game);
extern VOID RotatePiece(GameState *game);
extern VOID HoldPiece(GameState *game);

// Translate key to game control
typedef enum {
  CMD_NONE,
  CMD_LEFT,
  CMD_RIGHT,
  CMD_SOFT_DROP,
  CMD_HARD_DROP,
  CMD_ROTATE,
  CMD_PAUSE,
  CMD_HOLD,
  CMD_START,
  CMD_QUIT
} GameCommand;

// Initialize input
EFI_STATUS InitInput(IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn) {
  EFI_STATUS status;
  EFI_INPUT_KEY Key;

  // Reset the console input device
  status = ConIn->Reset(ConIn, FALSE);
  if (EFI_ERROR(status)) {
    return status;
  }

  // Clear any pending keys
  while (ConIn->ReadKeyStroke(ConIn, &Key) == EFI_SUCCESS) {
    // Discard
  }

  return EFI_SUCCESS;
}

// Simple key translation
GameCommand TranslateKey(EFI_INPUT_KEY *Key) {
  // Left arrow
  if (Key->ScanCode == SCAN_LEFT) {
    return CMD_LEFT;
  }
  // Right arrow
  if (Key->ScanCode == SCAN_RIGHT) {
    return CMD_RIGHT;
  }
  // Down arrow
  if (Key->ScanCode == SCAN_DOWN) {
    return CMD_SOFT_DROP;
  }
  // Up arrow = rotate
  if (Key->ScanCode == SCAN_UP) {
    return CMD_ROTATE;
  }

  // Space = hard drop
  if (Key->UnicodeChar == L' ') {
    return CMD_HARD_DROP;
  }

  // P = pause
  if (Key->UnicodeChar == L'p' || Key->UnicodeChar == L'P') {
    return CMD_PAUSE;
  }

  // H = hold piece
  if (Key->UnicodeChar == L'h' || Key->UnicodeChar == L'H') {
    return CMD_HOLD;
  }

  // Enter = start
  if (Key->UnicodeChar == CHAR_CARRIAGE_RETURN) {
    return CMD_START;
  }

  // ESC = quit
  if (Key->ScanCode == SCAN_ESC) {
    return CMD_QUIT;
  }

  return CMD_NONE;
}

// Process game command
VOID ProcessCommand(GameState *game, GameCommand cmd) {
  switch (cmd) {
    case CMD_LEFT:
      if (!game->gameOver && !game->paused && game->started) {
        MovePiece(game, -1, 0);
      }
      break;

    case CMD_RIGHT:
      if (!game->gameOver && !game->paused && game->started) {
        MovePiece(game, 1, 0);
      }
      break;

    case CMD_SOFT_DROP:
      if (!game->gameOver && !game->paused && game->started) {
        SoftDrop(game);
      }
      break;

    case CMD_HARD_DROP:
      if (!game->gameOver && !game->paused && game->started) {
        HardDrop(game);
      }
      break;

    case CMD_ROTATE:
      if (!game->gameOver && !game->paused && game->started) {
        RotatePiece(game);
      }
      break;

    case CMD_PAUSE:
      if (!game->gameOver && game->started) {
        game->paused = !game->paused;
      }
      break;

    case CMD_HOLD:
      if (!game->gameOver && !game->paused && game->started) {
        HoldPiece(game);
      }
      break;

    case CMD_START:
      if (!game->started) {
        game->started = TRUE;
        SpawnNewPiece(game);
      } else if (game->gameOver) {
        // Restart
        InitGame(game);
        game->started = TRUE;
        SpawnNewPiece(game);
      }
      break;

    case CMD_QUIT:
      // Quit game - could return to shell
      break;

    default:
      break;
  }
}

// Handle key input
VOID HandleInput(GameState *game, IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn) {
  EFI_STATUS status;
  EFI_INPUT_KEY Key;

  // Try to read a key (non-blocking)
  status = ConIn->ReadKeyStroke(ConIn, &Key);

  if (status == EFI_SUCCESS) {
    GameCommand cmd = TranslateKey(&Key);
    if (cmd != CMD_NONE) {
      ProcessCommand(game, cmd);
    }
  }
}