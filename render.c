#include "types.h"
#include <Library/MemoryAllocationLib.h>

// Forward declaration
extern INT8 (*GetCurrentShape(GameState *game))[4];
extern INT8 (*GetShapeForPiece(INT8 piece, INT8 rotation))[4];

// Screen size (obtained at runtime)
static UINTN g_screenWidth = 1024;
static UINTN g_screenHeight = 768;

// GOP Protocol
static EFI_GRAPHICS_OUTPUT_PROTOCOL *g_gop = NULL;

// Double buffer for game area
#define GAME_AREA_WIDTH_PIXELS  (BOARD_WIDTH * BLOCK_SIZE)
#define GAME_AREA_HEIGHT_PIXELS (BOARD_HEIGHT * BLOCK_SIZE)
static EFI_GRAPHICS_OUTPUT_BLT_PIXEL *g_buffer = NULL;

// Helper: Convert RGB to EFI_GRAPHICS_OUTPUT_BLT_PIXEL
static EFI_GRAPHICS_OUTPUT_BLT_PIXEL RgbToBltPixel(RGB *color) {
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL pixel;
  pixel.Blue = color->Blue;
  pixel.Green = color->Green;
  pixel.Red = color->Red;
  pixel.Reserved = 0;
  return pixel;
}

// Simple 5x8 font data (digits and uppercase letters)
// Each character is 5 pixels wide, 8 pixels high
// Using bits 4-0 to represent pixels left to right
static const UINT8 g_fontData[][8] = {
  // Space (0x20) - 0x00
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  // ! (0x21) - 0x01
  {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x00},
  // " (0x22) - 0x02
  {0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00},
  // # (0x23) - 0x03
  {0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x00, 0x00, 0x00},
  // $ (0x24) - 0x04
  {0x04, 0x0E, 0x15, 0x0E, 0x14, 0x0E, 0x04, 0x00},
  // % (0x25) - 0x05
  {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x18, 0x00},
  // & (0x26) - 0x06
  {0x08, 0x14, 0x14, 0x08, 0x15, 0x12, 0x0D, 0x00},
  // ' (0x27) - 0x07
  {0x04, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00},
  // ( (0x28) - 0x08
  {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02, 0x00},
  // ) (0x29) - 0x09
  {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08, 0x00},
  // * (0x2A) - 0x0A
  {0x00, 0x04, 0x15, 0x0E, 0x15, 0x04, 0x00, 0x00},
  // + (0x2B) - 0x0B
  {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00, 0x00},
  // , (0x2C) - 0x0C
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08},
  // - (0x2D) - 0x0D
  {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00},
  // . (0x2E) - 0x0E
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06, 0x00},
  // / (0x2F) - 0x0F
  {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00, 0x00},
  // 0 (0x30) - 0x10
  {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E, 0x00},
  // 1 (0x31) - 0x11
  {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E, 0x00},
  // 2 (0x32) - 0x12
  {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F, 0x00},
  // 3 (0x33) - 0x13
  {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E, 0x00},
  // 4 (0x34) - 0x14
  {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02, 0x00},
  // 5 (0x35) - 0x15
  {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E, 0x00},
  // 6 (0x36) - 0x16
  {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E, 0x00},
  // 7 (0x37) - 0x17
  {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08, 0x00},
  // 8 (0x38) - 0x18
  {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E, 0x00},
  // 9 (0x39) - 0x19
  {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C, 0x00},
  // : (0x3A) - 0x1A
  {0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00, 0x00},
  // ; (0x3B) - 0x1B
  {0x00, 0x06, 0x06, 0x00, 0x06, 0x02, 0x04, 0x00},
  // < (0x3C) - 0x1C
  {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02, 0x00},
  // = (0x3D) - 0x1D
  {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00, 0x00},
  // > (0x3E) - 0x1E
  {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08, 0x00},
  // ? (0x3F) - 0x1F
  {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04, 0x00},
  // @ (0x40) - 0x20
  {0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E, 0x00},
  // A (0x41) - 0x21
  {0x04, 0x0A, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x00},
  // B (0x42) - 0x22
  {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E, 0x00},
  // C (0x43) - 0x23
  {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E, 0x00},
  // D (0x44) - 0x24
  {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C, 0x00},
  // E (0x45) - 0x25
  {0x1F, 0x10, 0x10, 0x1C, 0x10, 0x10, 0x1F, 0x00},
  // F (0x46) - 0x26
  {0x1F, 0x10, 0x10, 0x1C, 0x10, 0x10, 0x10, 0x00},
  // G (0x47) - 0x27
  {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F, 0x00},
  // H (0x48) - 0x28
  {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x00},
  // I (0x49) - 0x29
  {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E, 0x00},
  // J (0x4A) - 0x2A
  {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0E, 0x00},
  // K (0x4B) - 0x2B
  {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11, 0x00},
  // L (0x4C) - 0x2C
  {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F, 0x00},
  // M (0x4D) - 0x2D
  {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11, 0x00},
  // N (0x4E) - 0x2E
  {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11, 0x00},
  // O (0x4F) - 0x2F
  {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00},
  // P (0x50) - 0x30
  {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10, 0x00},
  // Q (0x51) - 0x31
  {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D, 0x00},
  // R (0x52) - 0x32
  {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11, 0x00},
  // S (0x53) - 0x33
  {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E, 0x00},
  // T (0x54) - 0x34
  {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00},
  // U (0x55) - 0x35
  {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x00},
  // V (0x56) - 0x36
  {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04, 0x00},
  // W (0x57) - 0x37
  {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11, 0x00},
  // X (0x58) - 0x38
  {0x11, 0x0A, 0x04, 0x04, 0x04, 0x0A, 0x11, 0x00},
  // Y (0x59) - 0x39
  {0x11, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00},
  // Z (0x5A) - 0x3A
  {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F, 0x00},
};

// Initialize GOP
EFI_STATUS InitGOP(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop) {
  g_gop = Gop;

  // Get screen dimensions from current mode
  g_screenWidth = g_gop->Mode->Info->HorizontalResolution;
  g_screenHeight = g_gop->Mode->Info->VerticalResolution;

  // Debug: print screen size
  // Note: We can't use Print here easily, but we'll verify through rendering

  // Allocate double buffer for game area
  g_buffer = AllocatePool(GAME_AREA_WIDTH_PIXELS * GAME_AREA_HEIGHT_PIXELS * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
  if (g_buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Clear buffer to a test color (bright red) to verify buffer works
  {
    INT32 idx;
    INT32 total = GAME_AREA_WIDTH_PIXELS * GAME_AREA_HEIGHT_PIXELS;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL testPixel = {0, 0, 255, 0}; // Red
    for (idx = 0; idx < total; idx++) {
      g_buffer[idx] = testPixel;
    }
  }

  return EFI_SUCCESS;
}

// Set pixel color using Blt
VOID SetPixel(INT32 x, INT32 y, RGB *color) {
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL pixel;

  if (g_gop == NULL) return;
  if (x < 0 || x >= (INT32)g_screenWidth || y < 0 || y >= (INT32)g_screenHeight) {
    return;
  }

  pixel.Blue = color->Blue;
  pixel.Green = color->Green;
  pixel.Red = color->Red;
  pixel.Reserved = 0;

  g_gop->Blt(g_gop, &pixel, EfiBltVideoFill, 0, 0, x, y, 1, 1, 0);
}

// Draw rectangle using Blt
VOID DrawRect(INT32 x, INT32 y, INT32 width, INT32 height, RGB *color) {
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL pixel;

  if (g_gop == NULL || width <= 0 || height <= 0) return;
  if (x >= (INT32)g_screenWidth || y >= (INT32)g_screenHeight) return;

  // Clip to screen bounds
  if (x + width > (INT32)g_screenWidth) width = (INT32)g_screenWidth - x;
  if (y + height > (INT32)g_screenHeight) height = (INT32)g_screenHeight - y;
  if (x < 0) {
    width += x;
    x = 0;
  }
  if (y < 0) {
    height += y;
    y = 0;
  }
  if (width <= 0 || height <= 0) return;

  pixel.Blue = color->Blue;
  pixel.Green = color->Green;
  pixel.Red = color->Red;
  pixel.Reserved = 0;

  g_gop->Blt(g_gop, &pixel, EfiBltVideoFill, 0, 0, x, y, width, height, 0);
}

// Draw background with gradient decoration
VOID DrawGameBackground(VOID) {
  RGB bgColor = {20, 20, 35, 0};
  RGB borderColor = {80, 80, 120, 0};
  RGB shadowColor = {10, 10, 20, 0};

  // Shadow effect
  DrawRect(GAME_AREA_X + 4, GAME_AREA_Y + 4,
           BOARD_WIDTH * BLOCK_SIZE + 4, BOARD_HEIGHT * BLOCK_SIZE + 4,
           &shadowColor);

  // Game area border (thicker)
  DrawRect(GAME_AREA_X - 4, GAME_AREA_Y - 4,
           BOARD_WIDTH * BLOCK_SIZE + 8, BOARD_HEIGHT * BLOCK_SIZE + 8,
           &borderColor);

  DrawRect(GAME_AREA_X, GAME_AREA_Y,
           BOARD_WIDTH * BLOCK_SIZE, BOARD_HEIGHT * BLOCK_SIZE,
           &bgColor);
}

// Draw info area background
VOID DrawInfoBackground(VOID) {
  RGB bgColor = {30, 30, 50, 0};
  RGB borderColor = {70, 70, 100, 0};
  RGB shadowColor = {10, 10, 20, 0};

  // Shadow effect
  DrawRect(INFO_AREA_X + 4, INFO_AREA_Y + 4,
           INFO_AREA_WIDTH + 4, INFO_AREA_HEIGHT + 4,
           &shadowColor);

  // Info area border
  DrawRect(INFO_AREA_X - 4, INFO_AREA_Y - 4,
           INFO_AREA_WIDTH + 8, INFO_AREA_HEIGHT + 8,
           &borderColor);

  DrawRect(INFO_AREA_X, INFO_AREA_Y,
           INFO_AREA_WIDTH, INFO_AREA_HEIGHT,
           &bgColor);
}

// Draw single block with gradient effect (for screen - direct)
VOID DrawBlock(INT32 x, INT32 y, RGB *color) {
  INT32 px = x * BLOCK_SIZE + GAME_AREA_X;
  INT32 py = y * BLOCK_SIZE + GAME_AREA_Y;

  // Main block
  DrawRect(px, py, BLOCK_SIZE, BLOCK_SIZE, color);

  // Gradient effect - lighter top-left
  RGB lightColor = {
    color->Red > 205 ? 255 : color->Red + 50,
    color->Green > 205 ? 255 : color->Green + 50,
    color->Blue > 205 ? 255 : color->Blue + 50, 0
  };
  DrawRect(px, py, BLOCK_SIZE - 2, 2, &lightColor);
  DrawRect(px, py, 2, BLOCK_SIZE - 2, &lightColor);

  // Gradient effect - darker bottom-right
  RGB darkColor = {
    color->Red > 50 ? color->Red - 50 : 0,
    color->Green > 50 ? color->Green - 50 : 0,
    color->Blue > 50 ? color->Blue - 50 : 0, 0
  };
  DrawRect(px + BLOCK_SIZE - 2, py, 2, BLOCK_SIZE, &darkColor);
  DrawRect(px, py + BLOCK_SIZE - 2, BLOCK_SIZE, 2, &darkColor);

  // Inner highlight
  RGB innerColor = {
    color->Red > 230 ? 255 : color->Red + 25,
    color->Green > 230 ? 255 : color->Green + 25,
    color->Blue > 230 ? 255 : color->Blue + 25, 0
  };
  DrawRect(px + 3, py + 3, 3, 3, &innerColor);
}

// ============ Double Buffer Functions ============

// Fill buffer with background color
static VOID ClearBuffer(EFI_GRAPHICS_OUTPUT_BLT_PIXEL *color) {
  INT32 i;
  INT32 total = GAME_AREA_WIDTH_PIXELS * GAME_AREA_HEIGHT_PIXELS;
  for (i = 0; i < total; i++) {
    g_buffer[i] = *color;
  }
}

// Draw rectangle to buffer (local coordinates 0,0 = game area top-left)
static VOID DrawRectToBuffer(INT32 x, INT32 y, INT32 width, INT32 height, RGB *color) {
  INT32 i, j;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL pixel;

  if (width <= 0 || height <= 0) return;
  if (x >= GAME_AREA_WIDTH_PIXELS || y >= GAME_AREA_HEIGHT_PIXELS) return;

  // Clip to buffer bounds
  if (x + width > GAME_AREA_WIDTH_PIXELS) width = GAME_AREA_WIDTH_PIXELS - x;
  if (y + height > GAME_AREA_HEIGHT_PIXELS) height = GAME_AREA_HEIGHT_PIXELS - y;
  if (x < 0) { width += x; x = 0; }
  if (y < 0) { height += y; y = 0; }
  if (width <= 0 || height <= 0) return;

  pixel.Blue = color->Blue;
  pixel.Green = color->Green;
  pixel.Red = color->Red;
  pixel.Reserved = 0;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      g_buffer[(y + i) * GAME_AREA_WIDTH_PIXELS + (x + j)] = pixel;
    }
  }
}

// Draw grid lines to buffer
static VOID DrawGridToBuffer(VOID) {
  RGB gridColor = {40, 40, 55, 0};
  INT32 i;

  // Vertical lines
  for (i = 1; i < BOARD_WIDTH; i++) {
    DrawRectToBuffer(i * BLOCK_SIZE, 0, 1, BOARD_HEIGHT * BLOCK_SIZE, &gridColor);
  }

  // Horizontal lines
  for (i = 1; i < BOARD_HEIGHT; i++) {
    DrawRectToBuffer(0, i * BLOCK_SIZE, BOARD_WIDTH * BLOCK_SIZE, 1, &gridColor);
  }
}

// Draw single block to buffer with gradient effect
static VOID DrawBlockToBuffer(INT32 x, INT32 y, RGB *color) {
  INT32 px = x * BLOCK_SIZE;
  INT32 py = y * BLOCK_SIZE;

  // Main block
  DrawRectToBuffer(px, py, BLOCK_SIZE, BLOCK_SIZE, color);

  // Gradient effect - lighter top-left
  RGB lightColor = {
    color->Red > 205 ? 255 : color->Red + 50,
    color->Green > 205 ? 255 : color->Green + 50,
    color->Blue > 205 ? 255 : color->Blue + 50, 0
  };
  DrawRectToBuffer(px, py, BLOCK_SIZE - 2, 2, &lightColor);
  DrawRectToBuffer(px, py, 2, BLOCK_SIZE - 2, &lightColor);

  // Gradient effect - darker bottom-right
  RGB darkColor = {
    color->Red > 50 ? color->Red - 50 : 0,
    color->Green > 50 ? color->Green - 50 : 0,
    color->Blue > 50 ? color->Blue - 50 : 0, 0
  };
  DrawRectToBuffer(px + BLOCK_SIZE - 2, py, 2, BLOCK_SIZE, &darkColor);
  DrawRectToBuffer(px, py + BLOCK_SIZE - 2, BLOCK_SIZE, 2, &darkColor);

  // Inner highlight
  RGB innerColor = {
    color->Red > 230 ? 255 : color->Red + 25,
    color->Green > 230 ? 255 : color->Green + 25,
    color->Blue > 230 ? 255 : color->Blue + 25, 0
  };
  DrawRectToBuffer(px + 3, py + 3, 3, 3, &innerColor);
}

// Copy buffer to screen
static VOID FlushBuffer(VOID) {
  if (g_gop == NULL || g_buffer == NULL) return;

  g_gop->Blt(g_gop, g_buffer, EfiBltBufferToVideo,
             0, 0, GAME_AREA_X, GAME_AREA_Y,
             GAME_AREA_WIDTH_PIXELS, GAME_AREA_HEIGHT_PIXELS, 0);
}

// Draw game board to buffer
static VOID DrawBoardToBuffer(GameState *game) {
  INT32 i, j;
  INT8 piece;

  for (i = 0; i < BOARD_HEIGHT; i++) {
    for (j = 0; j < BOARD_WIDTH; j++) {
      piece = game->board[i][j];
      if (piece > 0) {
        DrawBlockToBuffer(j, i, &g_tetrominoColors[piece - 1]);
      }
    }
  }
}

// Draw ghost piece to buffer
static VOID DrawGhostPieceToBuffer(GameState *game) {
  if (game->currentPiece < 0) return;

  INT8 ghostY = game->pieceY;
  INT8 (*shape)[4] = GetCurrentShape(game);
  INT8 i, j;

  // Find drop position
  while (TRUE) {
    BOOLEAN canMove = TRUE;
    for (i = 0; i < 4 && canMove; i++) {
      for (j = 0; j < 4 && canMove; j++) {
        if (shape[i][j]) {
          INT32 newX = game->pieceX + j;
          INT32 newY = ghostY + 1 + i;

          if (newY >= BOARD_HEIGHT || (newY >= 0 && game->board[newY][newX] != 0)) {
            canMove = FALSE;
          }
        }
      }
    }

    if (canMove) {
      ghostY++;
    } else {
      break;
    }
  }

  // Draw ghost piece
  if (ghostY != game->pieceY) {
    RGB ghostColor = {50, 50, 70, 0};
    for (i = 0; i < 4; i++) {
      for (j = 0; j < 4; j++) {
        if (shape[i][j]) {
          INT32 boardX = game->pieceX + j;
          INT32 boardY = ghostY + i;

          if (boardY >= 0 && boardY < BOARD_HEIGHT && boardX >= 0 && boardX < BOARD_WIDTH) {
            // Draw ghost as outline
            DrawRectToBuffer(boardX * BLOCK_SIZE, boardY * BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, &ghostColor);
          }
        }
      }
    }
  }
}

// Draw current piece to buffer
static VOID DrawCurrentPieceToBuffer(GameState *game) {
  if (game->currentPiece < 0) return;

  INT8 (*shape)[4] = GetCurrentShape(game);
  INT8 i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      if (shape[i][j]) {
        INT32 boardX = game->pieceX + j;
        INT32 boardY = game->pieceY + i;

        if (boardY >= 0 && boardY < BOARD_HEIGHT && boardX >= 0 && boardX < BOARD_WIDTH) {
          DrawBlockToBuffer(boardX, boardY, &g_tetrominoColors[game->currentPiece]);
        }
      }
    }
  }
}

// ============ Preview and Hold Drawing ============

// Draw a small preview piece (for next/hold)
static VOID DrawPreviewPiece(INT32 baseX, INT32 baseY, INT8 piece, BOOLEAN dimmed) {
  if (piece < 0 || piece >= TETROMINO_COUNT) return;

  INT8 (*shape)[4] = GetShapeForPiece(piece, 0);
  if (shape == NULL) return;

  RGB *color = &g_tetrominoColors[piece];
  RGB dimmedColor;
  INT32 blockSize = 20;  // Smaller blocks for preview
  INT8 i, j;

  if (dimmed) {
    dimmedColor.Red = color->Red / 2;
    dimmedColor.Green = color->Green / 2;
    dimmedColor.Blue = color->Blue / 2;
    dimmedColor.Reserved = 0;
    color = &dimmedColor;
  }

  // Center the preview
  INT32 offsetX = 0, offsetY = 0;
  INT8 minX = 4, maxX = 0, minY = 4, maxY = 0;
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      if (shape[i][j]) {
        if (j < minX) minX = j;
        if (j > maxX) maxX = j;
        if (i < minY) minY = i;
        if (i > maxY) maxY = i;
      }
    }
  }
  offsetX = (4 - (maxX - minX + 1)) * blockSize / 2 - minX * blockSize;
  offsetY = (4 - (maxY - minY + 1)) * blockSize / 2 - minY * blockSize;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      if (shape[i][j]) {
        INT32 px = baseX + offsetX + j * blockSize;
        INT32 py = baseY + offsetY + i * blockSize;

        // Draw block
        DrawRect(px, py, blockSize, blockSize, color);

        // Simple border
        RGB borderColor = {color->Red / 3, color->Green / 3, color->Blue / 3, 0};
        DrawRect(px + blockSize - 2, py, 2, blockSize, &borderColor);
        DrawRect(px, py + blockSize - 2, blockSize, 2, &borderColor);
      }
    }
  }
}

// ============ Original Screen Functions (for UI) ============

// Draw game board
VOID DrawBoard(GameState *game) {
  INT32 i, j;
  INT8 piece;

  // Draw locked pieces first
  for (i = 0; i < BOARD_HEIGHT; i++) {
    for (j = 0; j < BOARD_WIDTH; j++) {
      piece = game->board[i][j];
      if (piece > 0) {
        DrawBlock(j, i, &g_tetrominoColors[piece - 1]);
      }
    }
  }
}

// Draw current piece
VOID DrawCurrentPiece(GameState *game) {
  if (game->currentPiece < 0) return;

  INT8 (*shape)[4] = GetCurrentShape(game);
  INT8 i, j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      if (shape[i][j]) {
        INT32 boardX = game->pieceX + j;
        INT32 boardY = game->pieceY + i;

        if (boardY >= 0 && boardY < BOARD_HEIGHT && boardX >= 0 && boardX < BOARD_WIDTH) {
          DrawBlock(boardX, boardY, &g_tetrominoColors[game->currentPiece]);
        }
      }
    }
  }
}

// Draw ghost piece (preview drop position)
VOID DrawGhostPiece(GameState *game) {
  if (game->currentPiece < 0) return;

  INT8 ghostY = game->pieceY;
  INT8 (*shape)[4] = GetCurrentShape(game);
  INT8 i, j;

  // Find drop position
  while (TRUE) {
    BOOLEAN canMove = TRUE;
    for (i = 0; i < 4 && canMove; i++) {
      for (j = 0; j < 4 && canMove; j++) {
        if (shape[i][j]) {
          INT32 newX = game->pieceX + j;
          INT32 newY = ghostY + 1 + i;

          if (newY >= BOARD_HEIGHT || (newY >= 0 && game->board[newY][newX] != 0)) {
            canMove = FALSE;
          }
        }
      }
    }

    if (canMove) {
      ghostY++;
    } else {
      break;
    }
  }

  // Draw ghost piece (gray for semi-transparent effect)
  if (ghostY != game->pieceY) {
    RGB ghostColor = {50, 50, 70, 0};
    for (i = 0; i < 4; i++) {
      for (j = 0; j < 4; j++) {
        if (shape[i][j]) {
          INT32 boardX = game->pieceX + j;
          INT32 boardY = ghostY + i;

          if (boardY >= 0 && boardY < BOARD_HEIGHT && boardX >= 0 && boardX < BOARD_WIDTH) {
            DrawBlock(boardX, boardY, &ghostColor);
          }
        }
      }
    }
  }
}

// Draw character
VOID DrawChar(INT32 x, INT32 y, CHAR8 c, RGB *color, INT32 scale) {
  if (c < 0x20 || c > 0x5A) return;  // Only support ASCII 0x20-0x5A

  UINT8 charIndex = c - 0x20;
  INT32 i, j;

  for (i = 0; i < 8; i++) {
    UINT8 line = g_fontData[charIndex][i];
    // Font uses bits 4-0 for 5 pixels (left to right)
    for (j = 0; j < 5; j++) {
      if (line & (0x10 >> j)) {
        DrawRect(x + j * scale, y + i * scale, scale, scale, color);
      }
    }
  }
}

// Draw number
VOID DrawNumber(INT32 x, INT32 y, UINT32 num, RGB *color, INT32 scale) {
  CHAR8 buffer[16];
  INT32 i = 0;

  if (num == 0) {
    buffer[i++] = '0';
  } else {
    while (num > 0) {
      buffer[i++] = '0' + (num % 10);
      num /= 10;
    }
  }

  INT32 startX = x;
  INT32 j;
  for (j = i - 1; j >= 0; j--) {
    DrawChar(startX, y, buffer[j], color, scale);
    startX += 6 * scale;  // Character width + spacing
  }
}

// Draw text
VOID DrawText(INT32 x, INT32 y, CHAR8 *text, RGB *color, INT32 scale) {
  INT32 pos = 0;
  INT32 curX = x;

  while (text[pos] != 0) {
    DrawChar(curX, y, text[pos], color, scale);
    curX += 6 * scale;
    pos++;
  }
}

// Draw score panel
VOID DrawScorePanel(GameState *game) {
  RGB yellow = {255, 255, 0, 0};
  RGB green = {0, 255, 0, 0};
  RGB orange = {255, 165, 0, 0};
  RGB cyan = {0, 255, 255, 0};
  RGB panelBg = {40, 40, 60, 0};
  RGB borderColor = {60, 60, 90, 0};

  // Score panel background
  DrawRect(INFO_AREA_X + 10, INFO_AREA_Y + 200, INFO_AREA_WIDTH - 20, 100, &panelBg);
  DrawRect(INFO_AREA_X + 9, INFO_AREA_Y + 199, INFO_AREA_WIDTH - 18, 1, &borderColor);
  DrawRect(INFO_AREA_X + 9, INFO_AREA_Y + 301, INFO_AREA_WIDTH - 18, 1, &borderColor);

  // Score
  DrawText(INFO_AREA_X + 20, INFO_AREA_Y + 215, "SCORE", &cyan, 1);
  DrawNumber(INFO_AREA_X + 20, INFO_AREA_Y + 235, game->score, &yellow, 2);

  // Lines
  DrawText(INFO_AREA_X + 150, INFO_AREA_Y + 215, "LINES", &cyan, 1);
  DrawNumber(INFO_AREA_X + 150, INFO_AREA_Y + 235, game->lines, &orange, 2);

  // Level
  DrawText(INFO_AREA_X + 20, INFO_AREA_Y + 265, "LEVEL", &cyan, 1);
  DrawNumber(INFO_AREA_X + 100, INFO_AREA_Y + 265, game->level, &green, 2);
}

// Draw score
VOID DrawScore(GameState *game) {
  RGB white = {255, 255, 255, 0};
  RGB yellow = {255, 255, 0, 0};
  RGB cyan = {0, 255, 255, 0};
  RGB panelBg = {35, 35, 55, 0};
  RGB borderColor = {55, 55, 85, 0};

  // Title
  DrawText(INFO_AREA_X + 70, INFO_AREA_Y + 15, "TETRIS", &cyan, 2);

  // Next piece panel
  DrawRect(INFO_AREA_X + 10, INFO_AREA_Y + 60, 120, 100, &panelBg);
  DrawRect(INFO_AREA_X + 9, INFO_AREA_Y + 59, 122, 1, &borderColor);
  DrawRect(INFO_AREA_X + 9, INFO_AREA_Y + 161, 122, 1, &borderColor);
  DrawText(INFO_AREA_X + 30, INFO_AREA_Y + 65, "NEXT", &white, 1);
  DrawPreviewPiece(INFO_AREA_X + 30, INFO_AREA_Y + 85, game->nextPiece, FALSE);

  // Hold piece panel
  DrawRect(INFO_AREA_X + 150, INFO_AREA_Y + 60, 120, 100, &panelBg);
  DrawRect(INFO_AREA_X + 149, INFO_AREA_Y + 59, 122, 1, &borderColor);
  DrawRect(INFO_AREA_X + 149, INFO_AREA_Y + 161, 122, 1, &borderColor);
  DrawText(INFO_AREA_X + 180, INFO_AREA_Y + 65, "HOLD", &white, 1);
  DrawPreviewPiece(INFO_AREA_X + 170, INFO_AREA_Y + 85, game->holdPiece, game->holdUsed);

  // Score panel
  DrawScorePanel(game);

  // Game state messages
  if (!game->started) {
    DrawText(INFO_AREA_X + 50, INFO_AREA_Y + 340, "PRESS ENTER", &white, 1);
    DrawText(INFO_AREA_X + 70, INFO_AREA_Y + 360, "TO START", &white, 1);
  } else if (game->paused) {
    DrawText(INFO_AREA_X + 80, INFO_AREA_Y + 340, "PAUSED", &yellow, 2);
  } else if (game->gameOver) {
    DrawText(INFO_AREA_X + 60, INFO_AREA_Y + 330, "GAME OVER", &white, 1);
    DrawText(INFO_AREA_X + 50, INFO_AREA_Y + 360, "PRESS ENTER", &white, 1);
    DrawText(INFO_AREA_X + 60, INFO_AREA_Y + 380, "TO RESTART", &white, 1);
  }
}

// Draw controls
VOID DrawControls(VOID) {
  RGB gray = {120, 120, 140, 0};
  RGB lightBlue = {100, 180, 255, 0};
  RGB panelBg = {35, 35, 55, 0};
  RGB borderColor = {55, 55, 85, 0};

  INT32 startY = INFO_AREA_Y + 420;

  // Controls panel background
  DrawRect(INFO_AREA_X + 10, startY, INFO_AREA_WIDTH - 20, 130, &panelBg);
  DrawRect(INFO_AREA_X + 9, startY - 1, INFO_AREA_WIDTH - 18, 1, &borderColor);

  DrawText(INFO_AREA_X + 90, startY + 10, "CONTROLS", &gray, 1);
  startY += 35;

  DrawText(INFO_AREA_X + 20, startY, "ARROWS:Move", &lightBlue, 1);
  startY += 18;

  DrawText(INFO_AREA_X + 20, startY, "UP:Rotate", &lightBlue, 1);
  startY += 18;

  DrawText(INFO_AREA_X + 20, startY, "SPACE:Drop", &lightBlue, 1);
  startY += 18;

  DrawText(INFO_AREA_X + 20, startY, "H:Hold", &lightBlue, 1);
  startY += 18;

  DrawText(INFO_AREA_X + 20, startY, "P:Pause", &lightBlue, 1);
  startY += 18;

  DrawText(INFO_AREA_X + 20, startY, "ESC:Quit", &lightBlue, 1);
}

// Static UI initialized flag
static BOOLEAN g_uiInitialized = FALSE;

// Render entire game screen
VOID RenderGame(GameState *game) {
  RGB black = {15, 15, 25, 0};
  RGB bgColor = {25, 25, 40, 0};
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL bgPixel;

  // Check if GOP is valid
  if (g_gop == NULL || g_buffer == NULL) {
    return;
  }

  // First time: draw static UI elements
  if (!g_uiInitialized) {
    // Clear screen once
    DrawRect(0, 0, (INT32)g_screenWidth, (INT32)g_screenHeight, &black);

    // Draw static backgrounds
    DrawGameBackground();
    DrawInfoBackground();

    // Draw controls (static)
    DrawControls();

    g_uiInitialized = TRUE;
  }

  // Clear buffer with background color
  bgPixel.Blue = bgColor.Blue;
  bgPixel.Green = bgColor.Green;
  bgPixel.Red = bgColor.Red;
  bgPixel.Reserved = 0;
  ClearBuffer(&bgPixel);

  // Draw grid lines
  DrawGridToBuffer();

  // Draw game board to buffer (locked pieces)
  DrawBoardToBuffer(game);

  // Draw ghost piece and current piece to buffer
  if (game->started && !game->gameOver) {
    DrawGhostPieceToBuffer(game);
    DrawCurrentPieceToBuffer(game);
  }

  // Copy buffer to screen (single Blt operation - no flicker!)
  FlushBuffer();

  // Draw score (UI area - direct to screen, doesn't flicker)
  DrawScore(game);
}