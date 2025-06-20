#pragma once  // Prevents multiple inclusions of this header file
//*************************************************************************
// Copyright(c) 2024 Dad Design.
//   Define here the parameters for your display and SPI interface
//*************************************************************************

//-------------------------------------------------------------------------
// Screen dimensions in pixels
#define TFT_WIDTH       240       // Screen width in pixels
#define TFT_HEIGHT      320       // Screen height in pixels

//-------------------------------------------------------------------------
// Display controller type
#define TFT_CONTROLEUR_TFT  7789  // Use the ILI7789 controller
//#define TFT_CONTROLEUR_TFT  7735  // Uncomment to use the ILI7735 controller


//-------------------------------------------------------------------------
// Color encoding
// 18-bit encoding: RGB666
// 16-bit encoding: RGB565

//-------------------------------------------------------------------------
//#define TFT_COLOR 18            // Uncomment for 18-bit color mode
#define TFT_COLOR 16              // Use 16-bit color mode (default)
#define INV_COLOR 				  // Screen displays with inverted colors – remove/comment this if not needed"

//-------------------------------------------------------------------------
// Define the block size used for partial screen updates
//   -> Only blocks that have been modified are sent to the display
// Note:
// - TFT_WIDTH must be evenly divisible by BLOC_WIDTH
// - TFT_HEIGHT must be evenly divisible by BLOC_HEIGHT
// Example: For a 240x320 screen, a block size of 24x32 is valid
#define NB_BLOC_WIDTH   10               // Number of blocks horizontally
#define NB_BLOC_HEIGHT  10               // Number of blocks vertically
#define NB_BLOCS        NB_BLOC_WIDTH * NB_BLOC_HEIGHT // Total number of blocks
#define BLOC_WIDTH      TFT_WIDTH / NB_BLOC_WIDTH      // Width of each block in pixels
#define BLOC_HEIGHT     TFT_HEIGHT / NB_BLOC_HEIGHT    // Height of each block in pixels


//-------------------------------------------------------------------------
// FIFO size for SPI block transmission via DMA
#define SIZE_FIFO 20  // Number of blocks in the FIFO buffer

//-------------------------------------------------------------------------
// If you want to use the DMA2D graphics accelerator, uncomment the following line.
#define USE_DMA2D
