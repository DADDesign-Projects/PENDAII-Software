#pragma once
//====================================================================================
// Effect.h
//
// PENDA Software framework configuration file for user effect.
// Copyright(c) 2025 Dad Design.
//====================================================================================

#define PENDA_DELAY
//#define PENDA_TREMOLO

// Configuring the PENDA Delay
#ifdef PENDA_DELAY
#include "Delay.h"
#define EFFECT DadEffect::cDelay
#define EFFECT_NAME "Delay"
#define EFFECT_VERSION "Version 1.0"
#endif
/*
// Configuring the PENDA Delay
#ifdef PENDA_TREMOLO
#include "Tremolo.h"
#define EFFECT DadEffect::cTremolo
#define EFFECT_NAME "Tremolo"
#define EFFECT_VERSION "Version 1.0"
#endif
*/
