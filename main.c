#include "types.h"

// External function declarations
extern EFI_STATUS InitGOP(IN EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop);
extern VOID RenderGame(GameState *game);
extern VOID HandleInput(GameState *game, IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn);
extern EFI_STATUS InitInput(IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn);
extern VOID DrawScore(GameState *game);
extern VOID DrawControls(VOID);
extern VOID InitGame(GameState *game);
extern VOID SoftDrop(GameState *game);

// Find and set the best graphics mode (prefer 1024x768)
static UINT32 FindBestMode(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop) {
  UINT32 modeIndex;
  UINT32 bestMode = 0;
  UINT32 bestWidth = 0;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
  UINTN infoSize;

  // Find 1024x768 or the largest available mode
  for (modeIndex = 0; modeIndex < gop->Mode->MaxMode; modeIndex++) {
    if (EFI_ERROR(gop->QueryMode(gop, modeIndex, &infoSize, &info))) {
      continue;
    }

    // Prefer 1024x768
    if (info->HorizontalResolution == 1024 && info->VerticalResolution == 768) {
      return modeIndex;
    }

    // Otherwise track the largest mode
    if (info->HorizontalResolution > bestWidth) {
      bestWidth = info->HorizontalResolution;
      bestMode = modeIndex;
    }
  }

  return bestMode;
}

// Main function
VOID TetrisMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL *conIn;
  GameState game;
  EFI_EVENT timerEvent;
  UINTN index;
  UINT32 timerCount = 0;
  UINT32 dropInterval;
  UINT32 bestMode;
  UINT32 originalMode;
  UINT32 i;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
  UINTN infoSize;

  // Initialize game state
  InitGame(&game);

  // Get GOP protocol
  status = SystemTable->BootServices->LocateProtocol(
    &gEfiGraphicsOutputProtocolGuid,
    NULL,
    (VOID **)&gop
  );
  if (EFI_ERROR(status)) {
    return;
  }

  // Print available modes for debugging
  SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Available GOP modes:\r\n");
  for (i = 0; i < gop->Mode->MaxMode; i++) {
    if (EFI_ERROR(gop->QueryMode(gop, i, &infoSize, &info))) {
      continue;
    }
    // Simple debug - just show mode number and resolution
    Print(L"  Mode %d: %dx%d\r\n", i, info->HorizontalResolution, info->VerticalResolution);
  }

  // Store original mode
  originalMode = gop->Mode->Mode;
  Print(L"Current mode: %d\r\n", originalMode);

  // Find and set best graphics mode (1024x768)
  bestMode = FindBestMode(gop);
  Print(L"Best mode selected: %d\r\n", bestMode);

  if (bestMode != originalMode) {
    status = gop->SetMode(gop, bestMode);
    Print(L"SetMode status: %r\r\n", status);
  } else {
    Print(L"Already in best mode\r\n");
  }

  // Initialize graphics
  status = InitGOP(gop);
  if (EFI_ERROR(status)) {
    return;
  }

  // Get console input protocol
  conIn = SystemTable->ConIn;

  // Initialize input (clear buffer)
  InitInput(conIn);

  // Create timer event (for game dropping)
  status = SystemTable->BootServices->CreateEvent(
    EVT_TIMER,
    TPL_CALLBACK,
    NULL,
    NULL,
    &timerEvent
  );
  if (EFI_ERROR(status)) {
    return;
  }

  // Set timer: every 50ms for more responsive input
  status = SystemTable->BootServices->SetTimer(
    timerEvent,
    TimerPeriodic,
    50 * 10000  // 50ms
  );
  if (EFI_ERROR(status)) {
    return;
  }

  // Set drop interval
  dropInterval = 10;  // 10 ticks * 50ms = 500ms per drop

  // Render initial screen
  RenderGame(&game);

  // Main game loop - poll for input on each timer tick
  while (TRUE) {
    // Wait for timer event only
    status = SystemTable->BootServices->WaitForEvent(
      1,
      &timerEvent,
      &index
    );

    if (EFI_ERROR(status)) {
      continue;
    }

    // Poll for input (non-blocking)
    HandleInput(&game, conIn);

    // Game logic
    if (game.started && !game.gameOver && !game.paused) {
      timerCount++;

      // Adjust drop speed based on level
      dropInterval = 10 - (game.level - 1);
      if (dropInterval < 2) dropInterval = 2;

      // Auto drop
      if (timerCount >= dropInterval) {
        SoftDrop(&game);
        timerCount = 0;
      }
    }

    // Render game (includes score drawing)
    RenderGame(&game);
  }

  // Never reaches here
}