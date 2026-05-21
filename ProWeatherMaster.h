#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Engine/TextureCube.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialParameterCollection.h"
#include "ProWeatherMaster.generated.h"

UCLASS()
class PROWEATHER_API AProWeatherMaster : public AActor
{
	GENERATED_BODY()

public:
	AProWeatherMaster();

	/** Timers */
	FTimerHandle RandomWeatherTimerHandle;
	FTimerHandle ThunderTimerHandle;

	/** Estado general */
	UPROPERTY(BlueprintReadOnly) bool bSnowIsComing = false;
	UPROPERTY(BlueprintReadOnly) bool bRainIsComing = false;
	UPROPERTY(BlueprintReadOnly) bool bIsNight = false;

	/** Config general */
	UPROPERTY(EditAnywhere, Category = "Weather | Core") float WeatherRandomTime = 60.0f;
	UPROPERTY(EditAnywhere, Category = "Weather | Core") bool AlwaysRain = false;
	UPROPERTY(EditAnywhere, Category = "Weather | Core") bool AlwaysSnow = false;
	UPROPERTY(EditAnywhere, Category = "Weather | Core") bool AlwaysSunny = false;
	UPROPERTY(EditAnywhere, Category = "Weather | Core") bool AlwaysDayTime = false;
	UPROPERTY(EditAnywhere, Category = "Weather | Core") bool AlwaysNight = false;

	/** Colores */
	UPROPERTY(EditAnywhere, Category = "Weather | Visuals") FLinearColor DarkClouds;

	/** Snow */
	UPROPERTY(EditAnywhere, Category = "Weather | Snow") float MaxSnowFalling = 20000.0f;
	UPROPERTY(EditAnywhere, Category = "Weather | Snow") float SnowBlendMax = 7.0f;
	UPROPERTY(EditAnywhere, Category = "Weather | Snow") FLinearColor SnowColorOfDay;
	UPROPERTY(EditAnywhere, Category = "Weather | Snow") FLinearColor SnowColorOfNight;

	/** Rain */
	UPROPERTY(EditAnywhere, Category = "Weather | Rain") FLinearColor RainColorOfDay;
	UPROPERTY(EditAnywhere, Category = "Weather | Rain") FLinearColor RainColorOfNight;
	UPROPERTY(EditAnywhere, Category = "Weather | Rain") float MaxRainFalling = 20000.0f;
	UPROPERTY(EditAnywhere, Category = "Weather | Rain") float MaxRainFogFalling = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Weather | Rain") float RainDropsBlendMax = 0.1f;
	UPROPERTY(EditAnywhere, Category = "Weather | Rain") float GetWetRoughness = 0.3f;
	UPROPERTY(EditAnywhere, Category = "Weather | Rain") float GetWetSpecular = 2.0f;
	UPROPERTY(EditAnywhere, Category = "Weather | Rain") float ThunderWaitingTime = 30.0f;
	UPROPERTY(EditAnywhere, Category = "Weather | Rain") float ThunderMinVolume = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Weather | Rain") float ThunderMaxVolume = 1.5f;

	/** Sun & Light */
	UPROPERTY(EditAnywhere, Category = "Weather | Sun") float SunIntensityOfSunny = 10.0f;
	UPROPERTY(EditAnywhere, Category = "Weather | Sun") float SunIntensityOfRainOrSnow = 3.0f;
	UPROPERTY(EditAnywhere, Category = "Weather | Sun") float SunTemperatureOfDaytime = 5000.0f;
	UPROPERTY(EditAnywhere, Category = "Weather | Sun") float SunTemperatureOfNight = 8000.0f;

	/** Fog */
	UPROPERTY(EditAnywhere, Category = "Weather | Fog") float FogDensityOfSunny = 0.02f;
	UPROPERTY(EditAnywhere, Category = "Weather | Fog") float FogDensityOfRainOrSnow = 0.05f;
	UPROPERTY(EditAnywhere, Category = "Weather | Fog") float FogHeightFalloffOfSunny = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Weather | Fog") float FogHeightFalloffOfRainOrSnow = 0.3f;

	/** Wind */
	UPROPERTY(EditAnywhere, Category = "Weather | Wind") float WindSpeedOfSnowOrRain = 6.0f;
	UPROPERTY(EditAnywhere, Category = "Weather | Wind") float WindSpeedOfSunny = 3.0f;
	UPROPERTY(EditAnywhere, Category = "Weather | Wind") float WindWeightOfSnowOrRain = 8.0f;
	UPROPERTY(EditAnywhere, Category = "Weather | Wind") float WindWeightOfSunny = 2.0f;

	/** Skylight */
	UPROPERTY(EditAnywhere, Category = "Weather | SkyLight") float SkyLightIntensityOfDay = 1.2f;
	UPROPERTY(EditAnywhere, Category = "Weather | SkyLight") float SkyLightIntensityOfNight = 1.0f;

	/** Core Functions */
	void SetupDefaults();
	void WeatherRandomFunc();
	void GetPlayerLocation();
	void ThunderAudioPlay();
protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	/** Componentes principales */
	UPROPERTY(EditAnywhere) UDirectionalLightComponent* DirectionalLight;
	UPROPERTY(EditAnywhere) USkyLightComponent* SkyLight;
	UPROPERTY(EditAnywhere) USkyAtmosphereComponent* SkyAtmosphere;
	UPROPERTY(EditAnywhere) UPostProcessComponent* PostProcessComp;
	UPROPERTY(EditAnywhere) UVolumetricCloudComponent* VolumetricCloud;
	UPROPERTY(EditAnywhere) UExponentialHeightFogComponent* ExponentialHeightFog;
	UPROPERTY(EditAnywhere) UStaticMeshComponent* SkySphere;

	/** Niagara FX */
	UPROPERTY(EditAnywhere) UNiagaraComponent* RainFX;
	UPROPERTY(EditAnywhere) UNiagaraComponent* SnowFX;

	/** Audio */
	UPROPERTY(EditAnywhere) UAudioComponent* RainAudio;
	UPROPERTY(EditAnywhere) UAudioComponent* ThunderAudio;
	UPROPERTY(EditAnywhere) UAudioComponent* SnowAudio;
	UPROPERTY(EditAnywhere) UAudioComponent* BirdsAudio;
	UPROPERTY(EditAnywhere) UAudioComponent* WindAudio;
	UPROPERTY(EditAnywhere) UAudioComponent* CicadaAudio;

	/** Sound cues (Asignados en el .cpp y/o editor) */
	USoundCue* RainSound;
	USoundCue* ThunderSound;
	USoundCue* SnowSound;
	USoundCue* BirdsSound;
	USoundCue* WindSound;
	USoundCue* CicadaSound;

	/** MPC */
	UMaterialParameterCollection* MPC_Weather;
	UMaterialParameterCollection* MPC_TreeWind;

	/** Curves */
	UPROPERTY(EditAnywhere) UCurveFloat* IsNightCurve;
	UPROPERTY(EditAnywhere) UCurveFloat* SunAngleCurve;
	UPROPERTY(EditAnywhere) UCurveFloat* SnowOrRainFallingCurve;
	UPROPERTY(EditAnywhere) UCurveFloat* RainFallingCurve;
	UPROPERTY(EditAnywhere) UCurveFloat* SnowFallingCurve;
	UPROPERTY(EditAnywhere) UCurveFloat* GetWetCurve;
	UPROPERTY(EditAnywhere) UCurveFloat* FrozenCurve;

	/** Timelines */
	UPROPERTY(EditAnywhere) UTimelineComponent* IsNightTimeLine;
	UPROPERTY(EditAnywhere) UTimelineComponent* SunTimeLine;
	UPROPERTY(EditAnywhere) UTimelineComponent* SnowOrRainFallingTimeLine;
	UPROPERTY(EditAnywhere) UTimelineComponent* RainFallingTimeLine;
	UPROPERTY(EditAnywhere) UTimelineComponent* SnowFallingTimeLine;
	UPROPERTY(EditAnywhere) UTimelineComponent* GetWetTimeLine;
	UPROPERTY(EditAnywhere) UTimelineComponent* FrozenTimeLine;

	/** Timeline callbacks */
	UFUNCTION() void UpdateIsNightTimeLine(float Output);
	UFUNCTION() void UpdateSunTimeLine(float Output);
	UFUNCTION() void UpdateSnowOrRainFallingTimeLine(float Output);
	UFUNCTION() void UpdateRainFallingTimeLine(float Output);
	UFUNCTION() void UpdateSnowFallingTimeLine(float Output);
	UFUNCTION() void UpdateGetWetTimeLine(float Output);
	UFUNCTION() void UpdateFrozenTimeLine(float Output);
};
