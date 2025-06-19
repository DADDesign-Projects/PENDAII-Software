#pragma once
//====================================================================================
// cVolume.h
//
// Volume control module for digital audio output.
// Uses a software SPI interface and timer for precise transmission.
//
// Copyright (c) 2025 Dad Design.
//====================================================================================

#include "main.h"
#include "cSoftSPI.h"
#include <algorithm>

namespace DadMisc {

// -----------------------------------------------------------------------
// Number of steps for smooth volume transitions (100 steps for gradual fade)
#define NB_STEP 100.0f

// -----------------------------------------------------------------------
// Union for 32-bit volume control
// Allows access to the full 32-bit volume value or to individual 8-bit volume channels.
// This enables efficient SPI transmission while maintaining channel-specific control.
union VolumeControl {
    uint32_t Volume;  // Full 32-bit value (for SPI transmission)

    struct {
        uint8_t Vol1;  // Left channel for Volume 1 (Wet Left)
        uint8_t Vol2;  // Right channel for Volume 1 (Wet Right)
        uint8_t Vol3;  // Left channel for Volume 2 (Dry Left)
        uint8_t Vol4;  // Right channel for Volume 2 (Dry Right)
    };
};

// -----------------------------------------------------------------------
// Structure for managing smooth volume transitions
// Contains target values, current floating-point values, increments, and counters
// for each of the 4 volume channels to enable gradual volume changes.
struct sVolume {
        float		TargetVol1;     // Target volume for channel 1 (0.0 to 1.0)
        float		fVol1;          // Current floating-point volume for channel 1
        float   	IncVol1;        // Increment per step for smooth transition
        uint16_t	CtChange1;      // Remaining steps for volume change

        float		TargetVol2;     // Target volume for channel 2 (0.0 to 1.0)
        float		fVol2;          // Current floating-point volume for channel 2
        float   	IncVol2;        // Increment per step for smooth transition
        uint16_t	CtChange2;      // Remaining steps for volume change

        float		TargetVol3;     // Target volume for channel 3 (0.0 to 1.0)
        float		fVol3;          // Current floating-point volume for channel 3
        float   	IncVol3;        // Increment per step for smooth transition
        uint16_t	CtChange3;      // Remaining steps for volume change

        float		TargetVol4;     // Target volume for channel 4 (0.0 to 1.0)
        float		fVol4;          // Current floating-point volume for channel 4
        float   	IncVol4;        // Increment per step for smooth transition
        uint16_t	CtChange4;      // Remaining steps for volume change
};

// -----------------------------------------------------------------------
// Enum for Dry/Wet volume behavior
// Defines how Dry/Wet volumes are managed when toggling the effect On/Off.
// This allows different audio processing behaviors for effect bypass scenarios.
enum class eDryWetMode
{
    DryOffWetOn,  // Dry muted / Wet active when effect is On; reversed when Off
    DryManual,    // Dry volume is controlled manually by the user
    DryAuto       // Dry volume is automatically adjusted based on effect state
};

//***********************************************************************************
// class cVolume
//
// Controls 4 independent volume channels via SPI.
// Uses software SPI to send values to a digital volume controller.
//
// Volume1 (Ch1-2): Used for Wet signal (Left/Right) - processed audio output
// Volume2 (Ch3-4): Used for Dry signal (Left/Right) - unprocessed audio bypass
//
// Features:
// - Smooth volume transitions to prevent audio artifacts
// - Dry/Wet mixing with equal-power crossfade curves
// - Multiple bypass modes for different audio processing scenarios
// - Hardware mute control via GPIO
// - Timer-based SPI transmission for real-time audio applications
//***********************************************************************************

class cVolume {
public:
	// -----------------------------------------------------------------------
	// Constructor
	// Initializes member variables to default states
	cVolume(){}

	// -----------------------------------------------------------------------
	// Initialization
	// Sets up the SPI interface and initializes all volume levels to 0.
	// Configures the hardware timer for periodic volume updates and SPI timing.
	// Params: phtim - Pointer to hardware timer handle for SPI timing
	//         DryWetMode - Initial dry/wet behavior mode
	void init(TIM_HandleTypeDef * phtim, eDryWetMode DryWetMode = eDryWetMode::DryAuto);

	// -----------------------------------------------------------------------
	// Sets and transmits Volume1 (Wet signal: Left/Right)
	// Updates the processed audio output volume levels.
	// Behavior depends on the current DryWetMode and effect On/Off state.
	// Params: VolumeLeft - Left channel volume (0-255)
	//         VolumeRight - Right channel volume (0-255)
	void Volume1Change(uint8_t VolumeLeft, uint8_t VolumeRight);

	// -----------------------------------------------------------------------
	// Sets and transmits Volume2 (Dry signal: Left/Right)
	// Updates the unprocessed audio bypass volume levels.
	// Used for direct audio passthrough when effect is bypassed.
	// Params: VolumeLeft - Left channel volume (0-255)
	//         VolumeRight - Right channel volume (0-255)
	void Volume2Change(uint8_t VolumeLeft, uint8_t VolumeRight);

	// -----------------------------------------------------------------------
	// Sets the Dry/Wet mix ratio (0.0 = full dry, 1.0 = full wet)
	// Implements equal-power crossfade curve for smooth audio transitions.
	// Automatically adjusts internal volumes based on the mix percentage.
	// Params: Mix - Mix ratio as percentage (0-100, where 0=dry, 100=wet)
	// Returns: Applied wet gain factor (clamped to [0.0, 1.0])
	float MixDryWet(float Mix);

	// -----------------------------------------------------------------------
	// Handles On/Off state changes (typically effect bypass)
	// Manages audio routing between wet (processed) and dry (bypass) signals.
	// Adjusts volume levels depending on the selected Dry/Wet mode.
	// Params: OnOff - New effect state (On/Off)
	void OnOffChange(eOnOff OnOff);

	// -----------------------------------------------------------------------
	// Changes the Dry/Wet behavior mode
	// Allows runtime switching between different bypass behaviors.
	// Params: DryWetMode - New dry/wet control mode
	void BypassModeChange(eDryWetMode DryWetMode) {
		m_DryWetMode = DryWetMode;
	}

	// -----------------------------------------------------------------------
	// Mutes all volume outputs
	// Uses hardware GPIO control for immediate audio muting.
	// Stores current values for later restoration.
	void MuteOn();

	// -----------------------------------------------------------------------
	// Restores volume outputs from memory
	// Unmutes audio output via hardware GPIO control.
	void MuteOff();

	// -----------------------------------------------------------------------
	// Timer Callback
	// Called periodically by hardware timer interrupt.
	// Activates the Soft SPI driver for the volume controllers
	// and handles gradual volume transitions (e.g., fades).
	// Must be called at regular intervals for proper operation.
	void TimerCallback();

protected:
	// -----------------------------------------------------------------------
	// Internal SPI interface (shared across instances)
	// Handles low-level communication with digital volume controller chip
	DadMisc::cSoftSPI m_SoftSPI;

	// -----------------------------------------------------------------------
	// Smoothly increments or decrements the current volume toward target.
	// Prevents audio artifacts by implementing gradual volume changes.
	// Params: current - Reference to current volume value
	//         target - Target volume value to reach
	void UpdateVolume(uint8_t& current, uint8_t target);

	// -----------------------------------------------------------------------
	// Gradually updates all volume channels toward their target values,
	// and transmits updated value via SPI.
	// Called periodically to implement smooth volume transitions.
	void RefreshVolumes();

	// -----------------------------------------------------------------------
	// Sets all 4 volume channels simultaneously
	// Initiates smooth transition to new volume levels for all channels.
	// Params: Left1/Right1 - Wet signal volumes (0-255)
	//         Left2/Right2 - Dry signal volumes (0-255)
	void setVolume(uint8_t Left1, uint8_t Right1, uint8_t Left2, uint8_t Right2);

	// -----------------------------------------------------------------------
	// Sets Volume1 channels (Wet Left/Right)
	// Initiates smooth transition for processed audio output channels.
	// Params: Left1/Right1 - Wet signal volumes (0-255)
	void setVolume1(uint8_t Left1, uint8_t Right1);

	// -----------------------------------------------------------------------
	// Sets Volume2 channels (Dry Left/Right)
	// Initiates smooth transition for bypass audio channels.
	// Params: Left2/Right2 - Dry signal volumes (0-255)
	void setVolume2(uint8_t Left2, uint8_t Right2);

	// -----------------------------------------------------------------------
	// Member data

	VolumeControl  m_MemVolumes;   // Stored volume values (for mute/unmute restoration)
	sVolume		   m_TargetVolume; // Target volume structure for smooth transitions

	float          m_MixDryWet;    // Current dry/wet mix ratio (0.0 = dry, 1.0 = wet)
	eOnOff         m_MemOnOff;     // Current On/Off state of the effect
	eDryWetMode    m_DryWetMode;   // Current dry/wet control behavior mode
	uint16_t	   m_CtMaj;        // Main counter for timing operations (e.g., fade timing)
};

} // namespace DadMisc
