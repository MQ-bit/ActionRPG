// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ARHitCameraShake.h"

#include "Shakes/WaveOscillatorCameraShakePattern.h"

UARHitCameraShake::UARHitCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UWaveOscillatorCameraShakePattern* Pattern = CreateDefaultSubobject<UWaveOscillatorCameraShakePattern>(TEXT("Pattern"));
	Pattern->Duration = 0.18f;
	Pattern->BlendInTime = 0.f;
	Pattern->BlendOutTime = 0.1f;
	Pattern->Pitch.Amplitude = 0.8f;
	Pattern->Pitch.Frequency = 22.f;
	Pattern->Roll.Amplitude = 0.4f;
	Pattern->Roll.Frequency = 18.f;
	Pattern->X.Amplitude = 0.f;
	Pattern->Y.Amplitude = 0.f;
	Pattern->Z.Amplitude = 0.f;
	Pattern->Yaw.Amplitude = 0.f;
	Pattern->FOV.Amplitude = 0.f;
	SetRootShakePattern(Pattern);
}
