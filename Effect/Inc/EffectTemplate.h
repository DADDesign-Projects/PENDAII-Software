#pragma once
//====================================================================================
// EffectTemplate.h
//
// Generic base class for an audio effect.
// Serves as a starting point for designing custom effects.
//
// Copyright (c) 2025 Dad Design.
//====================================================================================
#include "main.h"
#include "PendaUI.h"
#include "UIComponent.h"
#include "Parameter.h"
#include "UISystem.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmultichar"
// Serialization ID - must be changed with a unique code for each new effect
constexpr uint32_t EffectTemplateSerializeID = 'Tem0';
#pragma GCC diagnostic pop

namespace DadEffect {

//***********************************************************************************
//  cEffectTemplate
//
//  Base class for an audio effect on the PENDA platform.
//  Contains essential parameters, user interface elements, and audio processing.
//***********************************************************************************
class cEffectTemplate {
public:
	// --------------------------------------------------------------------------
	// Constructor
	cEffectTemplate() {}

	// --------------------------------------------------------------------------
	// Initializes DSP modules and UI parameters.
	virtual void Initialize();

	// --------------------------------------------------------------------------
	// Audio processing function.
	// Must be completed to implement a specific effect.
	virtual ITCM void Process(AudioBuffer *pIn, AudioBuffer *pOut, bool OnOff);

	// --------------------------------------------------------------------------
	// UI callback for the Dry/Wet mix parameter
	static void MixChange(DadUI::cParameter *pParameter, uint32_t CallbackUserData);

protected:
	// ==============================================================================
	// User Parameters (UI)
	// ==============================================================================

	// Parameter
	DadUI::cParameter m_DryWetMix;

	// Associated view widgets
	DadUI::cParameterNumNormalView m_DryWetMixView;

	// UI Groups
	DadUI::cUIParameters  m_ItemEffectMenu;    // Main effect parameters
	DadUI::cUIMemory      m_ItemMenuMemory;    // Persistent parameters
	DadUI::cUIImputVolume m_ItemInputVolume;   // Input volume control

	// Main menu container
	DadUI::cUIMenu m_Menu;

	// ==============================================================================
	// Internal DSP Components
	// ==============================================================================

	float m_GainWet;
};

} // namespace DadEffect
