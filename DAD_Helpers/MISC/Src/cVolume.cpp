//====================================================================================
// cVolume.cpp
//
// Volume control module for digital audio output.
// Uses a software SPI interface and hardware timer for precise transmission.
//
// Copyright (c) 2025 Dad Design.
//====================================================================================

#include "cVolume.h"
#include "PendaUI.h"

// -----------------------------------------------------------------------------
// Callback: HAL_TIM_PeriodElapsedCallback
// Purpose : Called by the HAL timer interrupt to handle SPI timing events.
//           This is the HAL library callback that gets invoked when the
//           hardware timer period elapses. It redirects to the static instance
//           of cPendaUI for volume refresh operations.
// Params  : htim - Pointer to the timer handle that triggered the interrupt
// Note    : This is a global callback function required by the HAL library
// -----------------------------------------------------------------------------
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
	// Redirect timer callback to the volume control system
	DadUI::cPendaUI::m_Volumes.TimerCallback();
}

namespace DadMisc {

//************************************************************************************
// Class: cVolume
//
// Manages 4 independent volume channels via software SPI.
// Provides mute control and dry/wet mixing logic for audio effects processing.
//
// Key Features:
// - Real-time volume control via SPI interface
// - Smooth volume transitions to prevent audio artifacts
// - Equal-power crossfade for dry/wet mixing
// - Multiple bypass modes for different audio processing scenarios
// - Hardware mute control for immediate audio cutoff
//************************************************************************************

// -----------------------------------------------------------------------------
// Method: init
// Purpose: Initializes the software SPI interface and sets all volume levels to 0.
//          Configures the system for audio volume control operations.
// Params : phtim - Pointer to the hardware timer handle (used by software SPI timing)
//          DryWetMode - Dry/Wet signal behavior mode (default: DryAuto)
// Note   : Must be called before any volume operations. Starts with muted output
//          for safe initialization.
// -----------------------------------------------------------------------------
void cVolume::init(TIM_HandleTypeDef* phtim, eDryWetMode DryWetMode) {
    // Initialize software SPI with GPIO pins and timer
    m_SoftSPI.Initialize(SSPI_DATA_GPIO_Port, SSPI_DATA_Pin,      // Data line
                         SSPI_CLK_GPIO_Port, SSPI_CLK_Pin,        // Clock line
                         SSPI_CS_GPIO_Port, SSPI_CS_Pin,          // Chip select
                         phtim);                                   // Timer for timing

    MuteOn();                   // Mute output on startup for safety

    // Reset all volume targets to zero for clean initialization
    m_TargetVolume = {0};

    // Clear memory volumes and send initial zero volume to hardware
    m_SoftSPI.Transmit(0);

    // Set default system states
    m_MixDryWet = 50.0f;                    // Default mix at 50% (balanced)
    m_MemOnOff = Off;                       // Default state is OFF (bypassed)
    m_DryWetMode = DryWetMode;              // Set initial Dry/Wet behavior mode
	m_CtMaj = 0;                            // Reset main timing counter

	// Start the hardware timer with interrupts for periodic callbacks
	HAL_TIM_Base_Start_IT(phtim);
}

// -----------------------------------------------------------------------------
// Method: Volume1Change
// Purpose: Updates volume for channels 1 and 2 (typically Wet Left/Right).
//          This controls the processed/effected audio output levels.
// Params : VolumeLeft  - Volume value for left channel (Vol1) [0-255]
//          VolumeRight - Volume value for right channel (Vol2) [0-255]
// Behavior: Depending on DryWetMode and OnOff state, applies volume to either
//           wet channels (when effect is on) or dry channels (when bypassed).
//           This allows seamless switching between processed and unprocessed audio.
// -----------------------------------------------------------------------------
void cVolume::Volume1Change(uint8_t VolumeLeft, uint8_t VolumeRight)
{
    // Store the input volumes in memory for state management
    m_MemVolumes.Vol1 = VolumeLeft;
    m_MemVolumes.Vol2 = VolumeRight;

    // Flags to determine which volume paths to update
    bool applyInputWetVolume = false;
    bool applyDryVolume = false;

    // Determine volume routing based on current dry/wet mode
    switch (m_DryWetMode)
    {
        case eDryWetMode::DryManual:
            // Manual mode: always apply to wet channels, dry is user-controlled
        	applyInputWetVolume = true;
            break;

        case eDryWetMode::DryOffWetOn:
            // Toggle mode: wet when on, dry when off (mutually exclusive)
            if (m_MemOnOff == On){
                applyInputWetVolume = true;    // Effect on: use wet path
    		}else{
    			applyInputWetVolume = false;   // Effect off: mute wet
    			applyDryVolume = true;         // Effect off: use dry path
    		}
            break;

        case eDryWetMode::DryAuto:
            // Auto mode: blend between wet and dry based on mix ratio
            if (m_MemOnOff == On){
                applyInputWetVolume = true;
                MixDryWet(m_MixDryWet);        // Apply crossfade mixing
            }else{
            	applyDryVolume = true;         // Effect off: dry only
            }
            break;
    }

    // Apply volume changes based on determined routing
    if (applyInputWetVolume){
    	setVolume1(m_MemVolumes.Vol1, m_MemVolumes.Vol2);
    }

    if (applyDryVolume){
        setVolume2(m_MemVolumes.Vol1, m_MemVolumes.Vol2); // Copy wet levels to dry
    }
}

// -----------------------------------------------------------------------------
// Method: Volume2Change
// Purpose: Updates volume for channels 3 and 4 (typically Dry Left/Right).
//          This controls the unprocessed/bypass audio output levels.
// Params : VolumeLeft  - Volume value for left channel (Vol3) [0-255]
//          VolumeRight - Volume value for right channel (Vol4) [0-255]
// Note   : This directly controls the dry signal path, used for audio bypass
//          when effects are turned off or for manual dry level control.
// -----------------------------------------------------------------------------
void cVolume::Volume2Change(uint8_t VolumeLeft, uint8_t VolumeRight){
    // Store dry volume levels in memory
    m_MemVolumes.Vol3 = VolumeLeft;
    m_MemVolumes.Vol4 = VolumeRight;

    // Apply dry volume levels immediately
    setVolume2(m_MemVolumes.Vol3, m_MemVolumes.Vol4);
}

// -----------------------------------------------------------------------------
// Method: MixDryWet
// Purpose: Computes the wet/dry balance using an equal-power crossfade curve.
//          Applies automatic attenuation to the dry signal path based on mix ratio
//          and current Volume1 levels. Returns the gain for the wet signal.
// Params : mixPercent - percentage of wet signal (0 = dry only, 100 = wet only)
// Returns: Linear gain factor to apply to the wet signal [0.0-1.0]
// Theory : Uses sine/cosine curves for equal-power crossfading, which maintains
//          constant perceived loudness during transitions between dry and wet.
// -----------------------------------------------------------------------------
#define VOLUME_STEP_SIZE 0.5f // Hardware volume controller step size: 0.5 dB per step

float cVolume::MixDryWet(float mixPercent) {
    // Store mix setting for state management
    m_MixDryWet = mixPercent;

    // Convert percentage to normalized range [0.0-1.0]
    float mixNormalized = mixPercent / 100.0f;

    // Clamp mix ratio to valid range
    if (mixNormalized > 1.0f) mixNormalized = 1.0f;
    if (mixNormalized < 0.0f) mixNormalized = 0.0f;

    // Equal-power crossfade curve (maintains constant power)
    // Wet increases with sine curve, dry decreases with cosine curve
    float wetGain = sinf(mixNormalized * M_PI / 2.0f);    // 0° to 90°
    float dryGain = cosf(mixNormalized * M_PI / 2.0f);    // 90° to 0°

    int16_t dryVolume1, dryVolume2;

    if (dryGain <= 0.001f) {
        // Dry gain too small: fully mute dry path to avoid noise
        dryVolume1 = 0;
        dryVolume2 = 0;
    } else {
        // Convert linear dry gain to logarithmic attenuation in dB
        float attenuationDB = 20.0f * log10f(dryGain);

        // Convert dB attenuation to hardware volume steps
        int16_t attenuationSteps = static_cast<int16_t>((attenuationDB / VOLUME_STEP_SIZE) + 0.5f);

        // Apply attenuation to stored volume levels
        dryVolume1 = m_MemVolumes.Vol1 + attenuationSteps;  // Note: attenuation is negative
        dryVolume2 = m_MemVolumes.Vol2 + attenuationSteps;

        // Clamp to minimum volume (prevent negative values)
        if (dryVolume1 < 0) dryVolume1 = 0;
        if (dryVolume2 < 0) dryVolume2 = 0;
    }

    // Update stored dry volumes with calculated attenuation
    m_MemVolumes.Vol3 = static_cast<uint8_t>(dryVolume1);
    m_MemVolumes.Vol4 = static_cast<uint8_t>(dryVolume2);

    // Apply updated dry volume only if effect is currently active
    if(m_MemOnOff == On){
    	setVolume2(m_MemVolumes.Vol3, m_MemVolumes.Vol4);
    }

    // Return wet gain for external use (e.g., digital signal processing)
    return wetGain;
}

// -----------------------------------------------------------------------------
// Method: OnOffChange
// Purpose: Handles the behavior of the dry/wet audio paths when the effect
//          is toggled ON/OFF. Manages audio routing and volume levels based
//          on the current dry/wet mode configuration.
// Params : OnOff - new ON/OFF state of the effect
// Note   : This is typically called when user presses bypass/effect toggle
// -----------------------------------------------------------------------------
void cVolume::OnOffChange(eOnOff OnOff)
{
    // Store the new effect state
    m_MemOnOff = OnOff;

    // Handle volume routing based on dry/wet mode
    switch (m_DryWetMode){

        case eDryWetMode::DryAuto:
            // Auto mode: blend wet/dry when on, dry only when off
            if (OnOff == On){
                MixDryWet(m_MixDryWet);        // Apply current mix ratio
            	setVolume1(m_MemVolumes.Vol1, m_MemVolumes.Vol2);  // Enable wet
            }else{
            	setVolume(0, 0, m_MemVolumes.Vol1, m_MemVolumes.Vol2); // Wet=0, Dry=input
            }
            break;

        case eDryWetMode::DryOffWetOn:
            // Toggle mode: mutually exclusive wet/dry operation
            if (OnOff == On){
                setVolume(m_MemVolumes.Vol1, m_MemVolumes.Vol2, 0, 0); // Wet=input, Dry=0
            }else{
                setVolume(0, 0, m_MemVolumes.Vol1, m_MemVolumes.Vol2); // Wet=0, Dry=input
            }
            break;

        case eDryWetMode::DryManual:
            // Manual mode: no automatic handling
            // User must manually call Volume1Change/Volume2Change as needed
            break;
    }
}

// -----------------------------------------------------------------------------
// Method: MuteOn
// Purpose: Immediately mutes the audio output using hardware GPIO control.
//          This provides instant audio cutoff for safety or user control.
// Note   : Uses GPIO pin to control external mute circuitry. Volume levels
//          are preserved in memory for restoration when unmuted.
// -----------------------------------------------------------------------------
void cVolume::MuteOn() {
    // Set mute GPIO low to activate hardware mute
    HAL_GPIO_WritePin(AUDIO_MUTE_GPIO_Port, AUDIO_MUTE_Pin, GPIO_PIN_RESET);
}

// -----------------------------------------------------------------------------
// Method: MuteOff
// Purpose: Restores audio output by deactivating hardware mute control.
//          Volume levels return to their previous states.
// Note   : Audio will resume at previously set volume levels
// -----------------------------------------------------------------------------
void cVolume::MuteOff() {
    // Set mute GPIO high to deactivate hardware mute
    HAL_GPIO_WritePin(AUDIO_MUTE_GPIO_Port, AUDIO_MUTE_Pin, GPIO_PIN_SET);
}

// -----------------------------------------------------------------------------
// Method: TimerCallback
// Purpose: Called periodically by hardware timer interrupt. Handles two main tasks:
//          1. SPI timing for communication with volume controller
//          2. Smooth volume transitions to prevent audio artifacts
// Note   : Critical for real-time audio operation. Must be called at regular
//          intervals (typically every few milliseconds) for proper operation.
// -----------------------------------------------------------------------------
void cVolume::TimerCallback(){
	m_SoftSPI.TimerCallback();  // Handle SPI state machine timing

	// Increment main counter for volume update timing
	m_CtMaj++;
	if(m_CtMaj > 200){          // Update volumes every 200 timer cycles
		m_CtMaj = 0;
		RefreshVolumes();       // Apply gradual volume changes
	}
}

// -----------------------------------------------------------------------------
// Method: RefreshVolumes
// Purpose: Gradually updates all volume channels toward their target values
//          using smooth interpolation. Transmits updated values via SPI when
//          any channel changes. This prevents audio artifacts (clicks/pops)
//          that would occur with instantaneous volume changes.
// Algorithm: Linear interpolation over NB_STEP iterations for each channel
// -----------------------------------------------------------------------------
void cVolume::RefreshVolumes(){
	VolumeControl  Volume;
	bool bTransmit = false;     // Flag to determine if SPI transmission needed

	// Process Volume 1 (Wet Left) smooth transition
	if(m_TargetVolume.CtChange1 > 0){
		m_TargetVolume.fVol1 += m_TargetVolume.IncVol1;  // Apply increment
		m_TargetVolume.CtChange1--;                      // Decrement step counter
		bTransmit = true;                                // Mark for transmission
	}

	// Process Volume 2 (Wet Right) smooth transition
	if(m_TargetVolume.CtChange2 > 0){
		m_TargetVolume.fVol2 += m_TargetVolume.IncVol2;
		m_TargetVolume.CtChange2--;
		bTransmit = true;
	}

	// Process Volume 3 (Dry Left) smooth transition
	if(m_TargetVolume.CtChange3 > 0){
		m_TargetVolume.fVol3 += m_TargetVolume.IncVol3;
		m_TargetVolume.CtChange3--;
		bTransmit = true;
	}

	// Process Volume 4 (Dry Right) smooth transition
	if(m_TargetVolume.CtChange4 > 0){
		m_TargetVolume.fVol4 += m_TargetVolume.IncVol4;
		m_TargetVolume.CtChange4--;
		bTransmit = true;
	}

	// If any channel changed, convert to hardware format and transmit
	if(bTransmit){
		// Convert normalized float values [0.0-1.0] to 8-bit hardware values [0-255]
		Volume.Vol1 = (uint8_t)(m_TargetVolume.fVol1 * 255.0f);
		Volume.Vol2 = (uint8_t)(m_TargetVolume.fVol2 * 255.0f);
		Volume.Vol3 = (uint8_t)(m_TargetVolume.fVol3 * 255.0f);
		Volume.Vol4 = (uint8_t)(m_TargetVolume.fVol4 * 255.0f);

		// Transmit all 4 channels as single 32-bit SPI transaction
		m_SoftSPI.Transmit(Volume.Volume);
	}
}

// -----------------------------------------------------------------------------
// Method: setVolume
// Purpose: Initiates smooth volume transitions for all 4 channels simultaneously.
//          Calculates incremental steps for gradual volume changes over NB_STEP
//          iterations to prevent audio artifacts.
// Params : Left1/Right1 - Target wet signal volumes [0-255]
//          Left2/Right2 - Target dry signal volumes [0-255]
// Algorithm: Linear interpolation from current to target over fixed step count
// -----------------------------------------------------------------------------
void cVolume::setVolume(uint8_t Left1, uint8_t Right1, uint8_t Left2, uint8_t Right2){
	// Convert 8-bit hardware values to normalized floating-point targets
	m_TargetVolume.TargetVol1 = Left1 / 255.0f;
	m_TargetVolume.TargetVol2 = Right1 / 255.0f;
	m_TargetVolume.TargetVol3 = Left2 / 255.0f;
	m_TargetVolume.TargetVol4 = Right2 / 255.0f;

	// Calculate incremental steps for smooth transitions
	// (target - current) / steps = increment per step
	m_TargetVolume.IncVol1 = (m_TargetVolume.TargetVol1 - m_TargetVolume.fVol1) / NB_STEP;
	m_TargetVolume.IncVol2 = (m_TargetVolume.TargetVol2 - m_TargetVolume.fVol2) / NB_STEP;
	m_TargetVolume.IncVol3 = (m_TargetVolume.TargetVol3 - m_TargetVolume.fVol3) / NB_STEP;
	m_TargetVolume.IncVol4 = (m_TargetVolume.TargetVol4 - m_TargetVolume.fVol4) / NB_STEP;

	// Initialize step counters for all channels
	m_TargetVolume.CtChange1 = NB_STEP;
	m_TargetVolume.CtChange2 = NB_STEP;
	m_TargetVolume.CtChange3 = NB_STEP;
	m_TargetVolume.CtChange4 = NB_STEP;
}

// -----------------------------------------------------------------------------
// Method: setVolume1
// Purpose: Initiates smooth volume transitions for wet channels (1-2) only.
//          Used when only the processed audio output levels need adjustment.
// Params : Left1/Right1 - Target wet signal volumes [0-255]
// Note   : Dry channels (3-4) remain unchanged
// -----------------------------------------------------------------------------
void cVolume::setVolume1(uint8_t Left1, uint8_t Right1){
	// Set targets for wet channels only
	m_TargetVolume.TargetVol1 = Left1 / 255.0f;
	m_TargetVolume.TargetVol2 = Right1 / 255.0f;

	// Calculate smooth transition increments
	m_TargetVolume.IncVol1 = (m_TargetVolume.TargetVol1 - m_TargetVolume.fVol1) / NB_STEP;
	m_TargetVolume.IncVol2 = (m_TargetVolume.TargetVol2 - m_TargetVolume.fVol2) / NB_STEP;

	// Initialize step counters for wet channels only
	m_TargetVolume.CtChange1 = NB_STEP;
	m_TargetVolume.CtChange2 = NB_STEP;
}

// -----------------------------------------------------------------------------
// Method: setVolume2
// Purpose: Initiates smooth volume transitions for dry channels (3-4) only.
//          Used when only the bypass audio output levels need adjustment.
// Params : Left2/Right2 - Target dry signal volumes [0-255]
// Note   : Wet channels (1-2) remain unchanged. This is typically used for
//          manual dry level control or bypass volume adjustments.
// -----------------------------------------------------------------------------
void cVolume::setVolume2(uint8_t Left2, uint8_t Right2){
	// Set targets for dry channels only
	m_TargetVolume.TargetVol3 = Left2 / 255.0f;
	m_TargetVolume.TargetVol4 = Right2 / 255.0f;

	// Calculate smooth transition increments
	m_TargetVolume.IncVol3 = (m_TargetVolume.TargetVol3 - m_TargetVolume.fVol3) / NB_STEP;
	m_TargetVolume.IncVol4 = (m_TargetVolume.TargetVol4 - m_TargetVolume.fVol4) / NB_STEP;

	// Initialize step counters for dry channels only
	m_TargetVolume.CtChange3 = NB_STEP;
	m_TargetVolume.CtChange4 = NB_STEP;
}

} // namespace DadMisc
