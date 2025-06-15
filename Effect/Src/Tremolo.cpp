//====================================================================================
// Tremolo.cpp
//
// Audio Tremolo Effect Module
//
// Copyright (c) 2025 Dad Design.
//====================================================================================

#include "Tremolo.h"

constexpr float DELAY_MAX_TIME = 0.02f; // Maximum modulation delay time in seconds
constexpr float FREQ_MIN = 0.5f;        // Minimum LFO frequency in Hz
constexpr float FREQ_MAX = 9.0f;        // Maximum LFO frequency in Hz

// Utility function to round a float up to the next unsigned integer
constexpr uint32_t ceil_to_uint(float value) {
    return static_cast<uint32_t>(value + 0.999f);
}

// Compute delay buffer size based on the sampling rate and max delay time
constexpr uint32_t DELAY_BUFFER_SIZE = ceil_to_uint(SAMPLING_RATE * DELAY_MAX_TIME);

// Allocate modulation delay buffers in external SDRAM (+100 samples for safe interpolation)
SDRAM_SECTION float __ModulationBufferLeft[DELAY_BUFFER_SIZE + 100];
SDRAM_SECTION float __ModulationBufferRight[DELAY_BUFFER_SIZE + 100];

namespace DadEffect {

//***********************************************************************************
//  cTremolo - Class handling the tremolo effect parameters, audio processing,
//             and user interface integration.
//***********************************************************************************

// --------------------------------------------------------------------------
// Initializes UI parameters, LFO, delay buffers, and menu interface
void cTremolo::Initialize(){

	// ---------------- Parameter Initialization ----------------

	// LFO Frequency
	m_Freq.Init(5.0f, FREQ_MIN, FREQ_MAX, 0.5f, 0.1f, SpeedChange, (uint32_t)this,
	            5.0f * UI_RT_SAMPLING_RATE, 20, TremoloSerializeID);

	// Tremolo Depth
	m_TremoloDeep.Init(45.0f, 0.0f, 100.0f, 5.0f, 1.0f, nullptr, 0,
	                   0.5f * UI_RT_SAMPLING_RATE, 21, TremoloSerializeID);
	// Dry/Wet Mix
#ifdef PENDAII
	m_DryWetMix.Init(45.0f, 0.0f, 100.0f, 5.0f, 1.0f, MixChange, (uint32_t)this,
	                   0.5f * UI_RT_SAMPLING_RATE, 22, TremoloSerializeID);
#endif

	// Vibrato Depth (used for delay-based pitch modulation)
	m_VibratoDeep.Init(0.0f, 0.0f, 100.0f, 5.0f, 1.0f, nullptr, 0,
	                   0.5f * UI_RT_SAMPLING_RATE, 23, TremoloSerializeID);

	// LFO Shape (0: Triangle, 1: Square)
	m_LFOShape.Init(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, nullptr, 0,
	                0.0f, 24, TremoloSerializeID);

	// LFO Duty Cycle Ratio
	m_LFORatio.Init(50.0f, 0.0f, 100.0f, 5.0f, 1.0f, RatioChange, (uint32_t)this,
	                0.5f * UI_RT_SAMPLING_RATE, 25, TremoloSerializeID);

	// Stereo mode
	m_StereoMode.Init(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, nullptr, 0, 0.0f, 26, TremoloSerializeID);


	// ---------------- View Setup ----------------
	m_FreqView.Init(&m_Freq, "Freq", "Frequency", "Hz", "Hz");
	m_TremoloDeepView.Init(&m_TremoloDeep, "Tremolo", "Tremolo depth", "%", "%");
	m_VibratoDeepView.Init(&m_VibratoDeep, "Vibrato", "Vibrato depth", "%", "%");
	m_DryWetMixView.Init(&m_DryWetMix, "Mix", "Dry/Wet", "%", "%");
	m_LFORatioView.Init(&m_LFORatio, "Ratio", "Ratio", "%", "%");
	m_LFOShapeView.Init(&m_LFOShape, "Shape", "Shape");
	m_LFOShapeView.AddDiscreteValue("Rect.", "Rectangular");
	m_LFOShapeView.AddDiscreteValue("Square", "Square");
	m_StereoModeView.Init(&m_StereoMode, "Stereo", "Stereo");
	m_StereoModeView.AddDiscreteValue("No", "No St. Effect");
	m_StereoModeView.AddDiscreteValue("Trem", "Tremolo St.");
	m_StereoModeView.AddDiscreteValue("Vibr", "Vibrato St.");
	m_StereoModeView.AddDiscreteValue("Both", "Both St.");

	// ---------------- Menu Grouping ----------------
	m_ItemTremoloMenu.Init(&m_TremoloDeepView, &m_VibratoDeepView, &m_DryWetMixView);
	m_ItemLFOMenu.Init(&m_LFOShapeView, &m_LFORatioView, &m_FreqView);
	m_ItemStereoMode.Init(&m_StereoModeView, nullptr, nullptr);
	m_ItemMenuMemory.Init(TremoloSerializeID);
	m_ItemInputVolume.Init();

	// ---------------- Main Menu Configuration ----------------
	m_Menu.Init();
	m_Menu.addMenuItem(&m_ItemTremoloMenu, "Main");
	m_Menu.addMenuItem(&m_ItemLFOMenu, "LFO");
	m_Menu.addMenuItem(&m_ItemStereoMode, "Stereo");
	m_Menu.addMenuItem(&m_ItemMenuMemory, "Mem.");
	m_Menu.addMenuItem(&m_ItemInputVolume, "Input");

	// Sync with footswitch for tap-tempo
	m_TapTempo.Init(&DadUI::cPendaUI::m_FootSwitch2, &m_FreqView, DadUI::eTempoType::frequency);

	// Activate the menu interface
	DadUI::cPendaUI::setActiveObject(&m_Menu);

	// ---------------- LFO and Delay Buffer Initialization ----------------
	m_LFOLeft.Initialize(SAMPLING_RATE, m_Freq, 1, 10, m_LFORatio.getNormalizedValue());
	m_LFORight.Initialize(SAMPLING_RATE, m_Freq, 1, 10, m_LFORatio.getNormalizedValue());
	m_LFORight.setPosition(0.5f);

	m_ModulationLineRight.Initialize(__ModulationBufferRight, DELAY_BUFFER_SIZE);
	m_ModulationLineRight.Clear();

	m_ModulationLineLeft.Initialize(__ModulationBufferLeft, DELAY_BUFFER_SIZE);
	m_ModulationLineLeft.Clear();

	DadUI::cPendaUI::m_Volumes.MuteOff();
}

// --------------------------------------------------------------------------
// Audio processing routine: applies volume and pitch modulation
void cTremolo::Process(AudioBuffer *pIn, AudioBuffer *pOut, bool OnOff){
	m_LFOLeft.Step(); // Update LFO phase
	m_LFORight.Step();
	m_ItemInputVolume.Process(pIn);		// Input volume VU-Meter

	float Left;
	float Right;
	if(false == OnOff){
		Left = 0.0f;
		Right = 0.0f;
	}else{
		Left = pIn->Left;
		Right = pIn->Right;
	}

	float VolumeModulationLeft = 0.0f;
	float VolumeModulationRight = 0.0f;
	float TremoloDeep = sinf((m_TremoloDeep / 100.0f) * M_PI / 2.0f);
	//float TremoloDeep = (m_TremoloDeep / 100.0f);
	switch(static_cast<uint32_t>(m_LFOShape.getValue())){
		case 0:
			if((m_StereoMode == 0) || (m_StereoMode == 2)){
				VolumeModulationLeft = sinf(1 - ((TremoloDeep)*(1-m_LFOLeft.getTriangleModValue()))* M_PI / 2.0f);
				VolumeModulationRight = VolumeModulationLeft;
			}else if((m_StereoMode == 1) || (m_StereoMode == 3)){
				VolumeModulationLeft = sinf(1 - ((TremoloDeep)*(1-m_LFOLeft.getTriangleModValue()))* M_PI / 2.0f);
				VolumeModulationRight = sinf(1 - ((TremoloDeep)*(1-m_LFORight.getTriangleModValue()))* M_PI / 2.0f);
			}
			break;
		case 1:
			if((m_StereoMode == 0) || (m_StereoMode == 2)){
				VolumeModulationLeft = 1 - ((TremoloDeep)*(1-m_LFOLeft.getSquareModValue()));
				VolumeModulationRight = VolumeModulationLeft;
			}else if((m_StereoMode == 1) || (m_StereoMode == 3)){
				VolumeModulationLeft = 1 - ((TremoloDeep)*(1-m_LFOLeft.getSquareModValue()));
				VolumeModulationRight = 1 - ((TremoloDeep)*(1-m_LFORight.getSquareModValue()));
			}
			break;
	}

	// Compute vibrato delay in samples.
	// The delay is modulated by the LFO sine wave, scaled by the user-defined depth,
	// and adjusted with a compensation factor to keep the vibrato range independent of LFO frequency.
	float LFOSinLeft = m_LFOLeft.getSineValue();
	float LFOSinRight = m_LFORight.getSineValue();
	float DelayLeft;
	float DelayRight;

	if((m_StereoMode == 0) || (m_StereoMode == 1)){
		DelayLeft = DELAY_BUFFER_SIZE * LFOSinLeft * m_CoefComp * (m_VibratoDeep/100)  * 0.5f;
		DelayRight = DelayLeft;
	}else if((m_StereoMode == 2) || (m_StereoMode == 3)){
		DelayLeft = DELAY_BUFFER_SIZE * LFOSinLeft * m_CoefComp * (m_VibratoDeep/100)  * 0.5f;
		DelayRight = DELAY_BUFFER_SIZE * LFOSinRight * m_CoefComp * (m_VibratoDeep/100)  * 0.5f;
	}

	// Push current samples to delay line and read modulated delayed output
	m_ModulationLineLeft.Push(Left);
	m_ModulationLineRight.Push(Right);
#ifdef PENDAI
	pOut->Left = m_ModulationLineLeft.Pull(DelayLeft) * VolumeModulationLeft;
	pOut->Right = m_ModulationLineRight.Pull(DelayLeft) * VolumeModulationLeft;
#elif defined(PENDAII)
	pOut->Left = m_ModulationLineLeft.Pull(DelayLeft) * VolumeModulationLeft * m_GainWet;
	pOut->Right = m_ModulationLineRight.Pull(DelayRight) * VolumeModulationRight * m_GainWet;
#endif
}

// --------------------------------------------------------------------------
// Callback to update the LFO frequency based on user interaction
void cTremolo::SpeedChange(DadUI::cParameter *pParameter, uint32_t CallbackUserData){
	cTremolo *pthis = reinterpret_cast<cTremolo *>(CallbackUserData);
	pthis->m_LFOLeft.setFreq(pParameter->getValue());
	pthis->m_LFORight.setFreq(pParameter->getValue());

	// Compute compensation factor to keep vibrato depth consistent across LFO frequencies.
	// This avoids a shallower vibrato effect when the LFO speed increases.
	pthis->m_CoefComp = (2 * FREQ_MIN) / (2 * (FREQ_MIN + (FREQ_MAX - FREQ_MIN) * pParameter->getNormalizedValue()));
}

// --------------------------------------------------------------------------
// Callback to update the LFO duty cycle ratio
void cTremolo::RatioChange(DadUI::cParameter *pParameter, uint32_t CallbackUserData){
	cTremolo *pthis = reinterpret_cast<cTremolo *>(CallbackUserData);
	pthis->m_LFOLeft.setNormalizedDutyCycle(pParameter->getNormalizedValue());
	pthis->m_LFORight.setNormalizedDutyCycle(pParameter->getNormalizedValue());
}
// --------------------------------------------------------------------------
// Callback to update the Tremolo Deep parameter
void cTremolo::MixChange(DadUI::cParameter *pParameter, uint32_t CallbackUserData){
	cTremolo *pthis = reinterpret_cast<cTremolo *>(CallbackUserData);
	pthis->m_GainWet = DadUI::cPendaUI::m_Volumes.MixDryWet(*pParameter);
}
} // namespace DadEffect
