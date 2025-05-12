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

namespace DadMisc {

// -----------------------------------------------------------------------
// Union for 32-bit volume control
// Allows access to the full 32-bit volume value or to individual 8-bit volume channels.
union VolumeControl {
    uint32_t Volume;  // Full 32-bit value (for SPI transmission)

    struct {
        uint8_t Vol1;  // Left channel for Volume 1
        uint8_t Vol2;  // Right channel for Volume 1
        uint8_t Vol3;  // Left channel for Volume 2
        uint8_t Vol4;  // Right channel for Volume 2
    };
};

//***********************************************************************************
// class cVolume
//
// Controls 4 independent volume channels via SPI.
// Uses software SPI to send values to a digital volume controller.
//***********************************************************************************

class cVolume {
public:
	// -----------------------------------------------------------------------
	// Constructor
	cVolume(){}

	// -----------------------------------------------------------------------
	// Initialization
	// Configures the SPI interface and sets all volume levels to 0.
	void init(TIM_HandleTypeDef * phtim);

	// -----------------------------------------------------------------------
	// Sets Volume1 (left/right) and transmits it
	void Volume1Change(float VolumeLeft, float VolumeRight);

	// -----------------------------------------------------------------------
	// Sets Volume2 (left/right) and transmits it
	void Volume2Change(float VolumeLeft, float VolumeRight);

	void MuteOn();

	void MuteOff();

	// -----------------------------------------------------------------------
	// Internal Data

	static DadMisc::cSoftSPI 	m_SoftSPI;       // Software SPI instance
protected:
	VolumeControl      			m_Volumes;       // Union holding 4 volume values
};

} // namespace DadMisc


