#pragma once
//====================================================================================
// Tremolo.h
//
// Declaration of the Tremolo effect class.
// Implements a tremolo effect with optional vibrato and a configurable LFO.
//
// Copyright (c) 2025 Dad Design.
//====================================================================================
#include "main.h"
#include "PendaUI.h"
#include "UIComponent.h"
#include "Parameter.h"
#include "cDCO.h"
#include "cDelayLine.h"
#include "UISystem.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmultichar"
constexpr uint32_t TremoloSerializeID ='Tre0'; // SerializeID for Delay Effect
#pragma GCC diagnostic pop

namespace DadEffect {

//***********************************************************************************
//  cTremolo
//
//  Main class for the Tremolo effect.
//  Handles parameter initialization, audio processing, LFO modulation,
//  vibrato (pitch modulation), and user interface integration.
//***********************************************************************************

class cTremolo {
public:
	// --------------------------------------------------------------------------
	// Constructor
	// Note: Does not perform any initialization. Call Initialize() explicitly.
	cTremolo() {};

	// --------------------------------------------------------------------------
	// Initializes DSP modules, LFO, delay buffers, and UI parameters.
	void Initialize();

	// --------------------------------------------------------------------------
	// Audio processing function
	// Applies tremolo and vibrato to the input audio buffer.
	ITCM void Process(AudioBuffer *pIn, AudioBuffer *pOut, bool OnOff);

	// --------------------------------------------------------------------------
	// UI Callbacks
	// Triggered when related parameters are changed by the user.
	static void SpeedChange(DadUI::cParameter *pParameter, uint32_t CallbackUserData);
	static void RatioChange(DadUI::cParameter *pParameter, uint32_t CallbackUserData);
	static void MixChange(DadUI::cParameter *pParameter, uint32_t CallbackUserData);

protected:

	// ==============================================================================
	// User Interface Components
	// ==============================================================================

	// DSP-linked parameters (user controlled)
	DadUI::cParameter m_TremoloDeep;    // Tremolo depth (% volume modulation)
	DadUI::cParameter m_VibratoDeep;    // Vibrato depth (% pitch modulation)
	DadUI::cParameter m_DryWetMix;    	// Mix dry/wet
	DadUI::cParameter m_LFOShape;       // LFO waveform shape (e.g., triangle, square)
	DadUI::cParameter m_Freq;           // LFO frequency (Hz)
	DadUI::cParameter m_LFORatio;       // LFO duty cycle (%)
	DadUI::cParameter m_StereoMode;     // Stereo Mode

	// Parameter views (UI widgets to display/edit parameters)
	DadUI::cParameterNumNormalView      m_FreqView;
	DadUI::cParameterNumNormalView      m_TremoloDeepView;
	DadUI::cParameterNumNormalView      m_VibratoDeepView;
	DadUI::cParameterNumNormalView		m_DryWetMixView;
	DadUI::cParameterDiscretView        m_LFOShapeView;
	DadUI::cParameterNumLeftRightView   m_LFORatioView;
	DadUI::cParameterDiscretView		m_StereoModeView;

	// UI parameter groupings (menu sections)
	DadUI::cUIParameters m_ItemTremoloMenu;   // Group for tremolo/vibrato parameters
	DadUI::cUIParameters m_ItemLFOMenu;       // Group for LFO shape and ratio
	DadUI::cUIParameters m_ItemStereoMode;	  // Group for Stereo mode
	DadUI::cUIMemory     m_ItemMenuMemory;    // Persistent parameter storage
	DadUI::cUIImputVolume m_ItemInputVolume;  // Input volume menu

	// Main menu container
	DadUI::cUIMenu m_Menu;

	// Tap tempo controller
	// Allows syncing LFO frequency to footswitch tempo.
	DadUI::cTapTempo m_TapTempo;

	// ==============================================================================
	// DSP Components
	// ==============================================================================

	DadDSP::cDCO m_LFOLeft;                 // Low-Frequency Oscillator for Left modulation
	DadDSP::cDCO m_LFORight;                // Low-Frequency Oscillator for Right modulation

	// Delay lines for vibrato (stereo processing)
	DadDSP::cDelayLine m_ModulationLineRight;
	DadDSP::cDelayLine m_ModulationLineLeft;
	float 			   m_GainWet;

	// Compensation factor to maintain vibrato depth regardless of LFO frequency
	float m_CoefComp = 0.0f;
};

} // namespace DadEffect
