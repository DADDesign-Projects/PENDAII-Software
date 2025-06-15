//====================================================================================
// cVolume.cpp
//
// Volume control module for digital audio output.
// Uses a software SPI interface and hardware timer for precise transmission.
//
// Copyright (c) 2025 Dad Design.
//====================================================================================

#include "cVolume.h"

DadMisc::cSoftSPI DadMisc::cVolume::m_SoftSPI;  // Software SPI instance (shared/static)

// -----------------------------------------------------------------------------
// Callback: HAL_TIM_PeriodElapsedCallback
// Called by the HAL timer (e.g., TIM6) interrupt.
// Delegates the timing tick to the software SPI handler.
//
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    DadMisc::cVolume::m_SoftSPI.TimerCallback();
}

namespace DadMisc {

//************************************************************************************
// Class: cVolume
//
// Manages 4 independent volume channels via software SPI.
// Provides mute control and dry/wet mixing logic.
//************************************************************************************

// -----------------------------------------------------------------------------
// Method: init
// Purpose: Initializes the software SPI interface and sets all volume levels to 0.
// Params : phtim - Pointer to the hardware timer handle (used by software SPI)
// -----------------------------------------------------------------------------
void cVolume::init(TIM_HandleTypeDef* phtim) {
    m_SoftSPI.Initialize(SSPI_DATA_GPIO_Port, SSPI_DATA_Pin,
                         SSPI_CLK_GPIO_Port, SSPI_CLK_Pin,
                         SSPI_CS_GPIO_Port, SSPI_CS_Pin,
                         phtim);

    MuteOn();                       		// Mute output on startup
    m_Volumes.Volume = 0x0;        			// Reset all volume channels
    m_SoftSPI.Transmit(m_Volumes.Volume);  	// Send initial state
    m_MixDryWet = 50.0f;
}

// -----------------------------------------------------------------------------
// Method: Volume1Change
// Purpose: Updates volume for channels 1 and 2 (typically left/right).
// Params : VolumeLeft  - volume value for channel 1
//          VolumeRight - volume value for channel 2
// -----------------------------------------------------------------------------
void cVolume::Volume1Change(float VolumeLeft, float VolumeRight) {
    m_Volumes.Vol1 = static_cast<uint8_t>(VolumeLeft);
    m_Volumes.Vol2 = static_cast<uint8_t>(VolumeRight);
    m_SoftSPI.Transmit(m_Volumes.Volume);
    MixDryWet(m_MixDryWet);
}

// -----------------------------------------------------------------------------
// Method: Volume2Change
// Purpose: Updates volume for channels 3 and 4 (e.g., dry path).
// Params : VolumeLeft  - volume value for channel 3
//          VolumeRight - volume value for channel 4
// -----------------------------------------------------------------------------
void cVolume::Volume2Change(float VolumeLeft, float VolumeRight) {
    m_Volumes.Vol3 = static_cast<uint8_t>(VolumeLeft);
    m_Volumes.Vol4 = static_cast<uint8_t>(VolumeRight);
    m_SoftSPI.Transmit(m_Volumes.Volume);
}

// -----------------------------------------------------------------------------
// Method: MixDryWet
// Purpose: Computes the wet/dry balance using an equal-power curve.
//          Applies attenuation to the dry signal path based on current Volume1,
//          and returns the gain for the wet path.
// Params : mixPercent - percentage of wet signal (0 = dry only, 100 = wet only)
// Returns: Linear gain to apply to the wet signal
// -----------------------------------------------------------------------------
#define VOLUME_STEP_SIZE 0.5f

float cVolume::MixDryWet(float mixPercent) {
    m_MixDryWet = mixPercent;
    float mixNormalized = mixPercent / 100.0f;

    // Clamp mix ratio
    if (mixNormalized > 1.0f) mixNormalized = 1.0f;
    if (mixNormalized < 0.0f) mixNormalized = 0.0f;

    // Equal-power crossfade curve
    float wetGain = sinf(mixNormalized * M_PI / 2.0f);
    float dryGain = cosf(mixNormalized * M_PI / 2.0f);

    int16_t dryVolume1, dryVolume2;

    if (dryGain <= 0.001f) {
        // Fully muted dry path
        dryVolume1 = 0;
        dryVolume2 = 0;
    } else {
        // Convert dry gain to attenuation in dB
        float attenuationDB = 20.0f * log10f(dryGain);
        int16_t attenuationSteps = static_cast<int16_t>((attenuationDB / VOLUME_STEP_SIZE) + 0.5f);

        dryVolume1 = m_Volumes.Vol1 + attenuationSteps;
        dryVolume2 = m_Volumes.Vol2 + attenuationSteps;

        // Clamp to 0
        if (dryVolume1 < 0) dryVolume1 = 0;
        if (dryVolume2 < 0) dryVolume2 = 0;
    }

    Volume2Change(static_cast<uint8_t>(dryVolume1), static_cast<uint8_t>(dryVolume2));
    return wetGain;
}

// -----------------------------------------------------------------------------
// Method: MuteOn
// Purpose: Mutes the audio output by setting mute GPIO low.
// -----------------------------------------------------------------------------
void cVolume::MuteOn() {
    HAL_GPIO_WritePin(AUDIO_MUTE_GPIO_Port, AUDIO_MUTE_Pin, GPIO_PIN_RESET);
}

// -----------------------------------------------------------------------------
// Method: MuteOff
// Purpose: Unmutes the audio output by setting mute GPIO high.
// -----------------------------------------------------------------------------
void cVolume::MuteOff() {
    HAL_GPIO_WritePin(AUDIO_MUTE_GPIO_Port, AUDIO_MUTE_Pin, GPIO_PIN_SET);
}

} // namespace DadMisc


