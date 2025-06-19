//====================================================================================
// EffectTemplate.cpp
//
// Implements the generic base class for an audio effect.
//
// Copyright (c) 2025 Dad Design.
//====================================================================================

#include "EffectTemplate.h"

namespace DadEffect {

// --------------------------------------------------------------------------
// Initializes UI parameters, gain, and menu interface
void cEffectTemplate::Initialize() {
	DadUI::cPendaUI::m_Volumes.BypassModeChange(DadMisc::eDryWetMode::DryAuto);
	m_GainWet = 0.0f;
	DadUI::cPendaUI::m_Volumes.MuteOn();

	// ---------------- Parameter Initialization ----------------
	m_DryWetMix.Init(
		50.0f, 0.0f, 100.0f, 5.0f, 1.0f,
		MixChange, (uint32_t)this,
		0.5f * UI_RT_SAMPLING_RATE, 20,
		EffectTemplateSerializeID
	);

	// ---------------- View Setup ----------------
	m_DryWetMixView.Init(&m_DryWetMix, "Mix", "Dry/Wet", "%", "%");

	// ---------------- Menu Grouping ----------------
	m_ItemEffectMenu.Init(nullptr, nullptr, &m_DryWetMixView);
	m_ItemMenuMemory.Init(EffectTemplateSerializeID);
	m_ItemInputVolume.Init();

	// ---------------- Main Menu Configuration ----------------
	m_Menu.Init();
	m_Menu.addMenuItem(&m_ItemEffectMenu, "Main");
	m_Menu.addMenuItem(&m_ItemMenuMemory, "Mem.");
	m_Menu.addMenuItem(&m_ItemInputVolume, "Input");

	// Activate the menu interface
	DadUI::cPendaUI::setActiveObject(&m_Menu);

	DadUI::cPendaUI::m_Volumes.MuteOff();
}

// --------------------------------------------------------------------------
// Audio processing routine (default passthrough with gain)
void cEffectTemplate::Process(AudioBuffer *pIn, AudioBuffer *pOut, bool OnOff) {
	m_ItemInputVolume.Process(pIn);  // Input volume and VU meter
#ifdef PENDAI
	pOut->Left = pIn->Left;
	pOut->Right = pIn->Right;
#elif defined(PENDAII)
	pOut->Left = pIn->Left * m_GainWet;
	pOut->Right = pIn->Right * m_GainWet;
#endif
}

// --------------------------------------------------------------------------
// Callback to update the Dry/Wet mix gain
void cEffectTemplate::MixChange(DadUI::cParameter *pParameter, uint32_t CallbackUserData) {
	cEffectTemplate *pthis = reinterpret_cast<cEffectTemplate *>(CallbackUserData);
	pthis->m_GainWet = DadUI::cPendaUI::m_Volumes.MixDryWet(*pParameter);
}

} // namespace DadEffect
