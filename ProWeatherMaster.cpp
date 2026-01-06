// ProWeatherMaster.cpp
#include "WeatherSystemMaster.h" // tu header existente
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/TextureCube.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Sound/SoundCue.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialParameterCollection.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Curves/CurveFloat.h"
#include "TimerManager.h"

// Constructor
AWeatherSystemMaster::AWeatherSystemMaster()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootComponent);

	// Directional Light
	DirectionalLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("DirectionalLight"));
	if (DirectionalLight)
	{
		DirectionalLight->SetupAttachment(RootComponent);
		DirectionalLight->SetMobility(EComponentMobility::Movable);
		DirectionalLight->SetRelativeRotation(FRotator(233.0f, 0.0f, 0.0f));
		DirectionalLight->bUseTemperature = true;
		DirectionalLight->Temperature = 4500.0f;
		DirectionalLight->bEnableLightShaftOcclusion = true;
		DirectionalLight->OcclusionMaskDarkness = 1.0f;
		DirectionalLight->bEnableLightShaftBloom = true;
		DirectionalLight->BloomScale = 0.5f;
		DirectionalLight->BloomMaxBrightness = 5.0f;
		DirectionalLight->bUsedAsAtmosphereSunLight = true;
		DirectionalLight->DynamicShadowDistanceMovableLight = 200000.0f;
		DirectionalLight->DynamicShadowCascades = 5.0f;
		DirectionalLight->CascadeDistributionExponent = 4.0f;
		DirectionalLight->CascadeTransitionFraction = 0.3f;
	}

	// Sky Light
	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	if (SkyLight)
	{
		SkyLight->SetupAttachment(RootComponent);
		SkyLight->SetMobility(EComponentMobility::Movable);
		SkyLight->Intensity = SkyLightIntensityOfDay;
		SkyLight->SourceType = SLS_SpecifiedCubemap;
		CubemapTemp = nullptr; // se puede asignar en editor o por carga segura abajo
	}

	// Sky Atmosphere
	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	if (SkyAtmosphere)
	{
		SkyAtmosphere->SetupAttachment(RootComponent);
		SkyAtmosphere->SetMobility(EComponentMobility::Movable);
		SkyAtmosphere->SetSkyLuminanceFactor(FVector(0.5f, 0.625f, 1.0f));
	}

	// Post Process
	PostProcessComp = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	if (PostProcessComp)
	{
		PostProcessComp->SetupAttachment(RootComponent);
		PostProcessComp->SetMobility(EComponentMobility::Movable);
		PostProcessComp->bUnbound = true;
		PostProcessComp->Settings.bOverride_BloomIntensity = true;
		PostProcessComp->Settings.BloomIntensity = 0.0f;
		PostProcessComp->Settings.bOverride_AutoExposureBias = true;
		PostProcessComp->Settings.AutoExposureBias = 0.0f;
		PostProcessComp->Settings.bOverride_VignetteIntensity = true;
		PostProcessComp->Settings.VignetteIntensity = 0.5f;
		PostProcessComp->Settings.bOverride_ColorGradingLUT = true;
		PostProcessComp->Settings.bOverride_ColorGradingIntensity = true;
		PostProcessComp->Settings.ColorGradingIntensity = 0.5f;
		LUT = nullptr;
	}

	// Volumetric Cloud
	VolumetricCloud = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("VolumetricCloud"));
	if (VolumetricCloud)
	{
		VolumetricCloud->SetupAttachment(RootComponent);
		VolumetricCloud->SetMobility(EComponentMobility::Movable);
		VolumetricCloud->LayerBottomAltitude = 20.0f;
		VolumetricCloud->bUsePerSampleAtmosphericLightTransmittance = true;
		CloudMaterial = nullptr;
	}

	// Exponential Height Fog
	ExponentialHeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("ExponentialHeightFog"));
	if (ExponentialHeightFog)
	{
		ExponentialHeightFog->SetupAttachment(RootComponent);
		ExponentialHeightFog->SetMobility(EComponentMobility::Movable);
		ExponentialHeightFog->SetRelativeLocation(FVector(0.0f, 0.0f, 5000.0f));
	}

	// Sky Sphere (static mesh)
	SkySphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkySphere"));
	if (SkySphere)
	{
		SkySphere->SetupAttachment(RootComponent);
		SkySphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SkySphere->CastShadow = false;
		SkySphereMesh = nullptr;
		SkyMaterial = nullptr;
	}

	// Rain FX (Niagara)
	RainFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RainFX"));
	if (RainFX)
	{
		RainFX->SetupAttachment(RootComponent);
		RainFX->SetVisibility(true);
		NS_RainFall = nullptr;
	}

	// Snow FX (Niagara)
	SnowFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SnowFX"));
	if (SnowFX)
	{
		SnowFX->SetupAttachment(RootComponent);
		SnowFX->SetVisibility(true);
		NS_SnowFall = nullptr;
	}

	// Audio Components (create but keep volume 0 by default where appropriate)
	RainAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RainAudio"));
	if (RainAudio) { RainAudio->SetupAttachment(RootComponent); RainAudio->bAutoActivate = false; RainAudio->VolumeMultiplier = 0.0f; RainSound = nullptr; }

	ThunderAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ThunderAudio"));
	if (ThunderAudio) { ThunderAudio->SetupAttachment(RootComponent); ThunderAudio->bAutoActivate = false; ThunderAudio->VolumeMultiplier = 0.0f; ThunderSound = nullptr; }

	SnowAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("SnowAudio"));
	if (SnowAudio) { SnowAudio->SetupAttachment(RootComponent); SnowAudio->bAutoActivate = false; SnowAudio->VolumeMultiplier = 0.0f; SnowSound = nullptr; }

	WindAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("WindAudio"));
	if (WindAudio) { WindAudio->SetupAttachment(RootComponent); WindAudio->bAutoActivate = false; WindAudio->VolumeMultiplier = 0.0f; WindSound = nullptr; }

	BirdsAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BirdsAudio"));
	if (BirdsAudio) { BirdsAudio->SetupAttachment(RootComponent); BirdsAudio->bAutoActivate = false; BirdsSound = nullptr; }

	CicadaAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("CicadaAudio"));
	if (CicadaAudio) { CicadaAudio->SetupAttachment(RootComponent); CicadaAudio->bAutoActivate = false; CicadaSound = nullptr; }

	// Timelines
	IsNightTimeLine = CreateDefaultSubobject<UTimelineComponent>(TEXT("IsNightTimeLine"));
	SunTimeLine = CreateDefaultSubobject<UTimelineComponent>(TEXT("SunTimeLine"));
	SnowOrRainFallingTimeLine = CreateDefaultSubobject<UTimelineComponent>(TEXT("SnowOrRainFallingTimeLine"));
	RainFallingTimeLine = CreateDefaultSubobject<UTimelineComponent>(TEXT("RainFallingTimeLine"));
	SnowFallingTimeLine = CreateDefaultSubobject<UTimelineComponent>(TEXT("SnowFallingTimeLine"));
	GetWetTimeLine = CreateDefaultSubobject<UTimelineComponent>(TEXT("GetWetTimeLine"));
	FrozenTimeLine = CreateDefaultSubobject<UTimelineComponent>(TEXT("FrozenTimeLine"));

	// Curves remain nullptr by default; safe checks later
	IsNightCurve = nullptr;
	SunAngleCurve = nullptr;
	SnowOrRainFallingCurve = nullptr;
	RainFallingCurve = nullptr;
	SnowFallingCurve = nullptr;
	GetWetCurve = nullptr;
	FrozenCurve = nullptr;

	// MPC references (optional to set here)
	MPC = nullptr;
	MPC_Tree = nullptr;

	// Default colors / values (can be overridden in editor)
	SnowColorOfDay = FLinearColor(0.5f, 0.65f, 1.0f, 1.0f);
	SnowColorOfNight = FLinearColor(0.25f, 0.325f, 0.5f, 1.0f);
	DarkClouds = FLinearColor(0.035f, 0.0395f, 0.05f, 1.0f);

	// Make sure defaults used in header are respected
	SkyLightIntensityOfDay = 1.2f;
	SkyLightIntensityOfNight = 10.0f;

	// Call setup hook for any extra safe initialization
	SetupDefaults();
}

// SetupDefaults(): cargado básico seguro de assets opcional (no obligatorio)
void AWeatherSystemMaster::SetupDefaults()
{
	// Intentionally conservative: hacemos cargas "optional" con checks.
	// Si quieres forzar assets, descomenta y ajusta las rutas con ConstructorHelpers.

	// Ejemplo seguro de carga (descomentar si quieres):
	// static ConstructorHelpers::FObjectFinder<UTextureCube> CubeFinder(TEXT("TextureCube'/WeatherSystem/HDRI/CubeMap.CubeMap'"));
	// if (CubeFinder.Succeeded()) { CubemapTemp = CubeFinder.Object; if (SkyLight) SkyLight->Cubemap = CubemapTemp; }

	// No forzamos llamadas que puedan fallar en tiempo de ejecución.
	// Dejamos las propiedades listas para que el diseñador asigne assets desde el editor.

	// Ajustes iniciales seguros
	if (SkyLight)
	{
		SkyLight->Intensity = SkyLightIntensityOfDay;
		if (CubemapTemp)
		{
			SkyLight->Cubemap = CubemapTemp;
		}
	}

	if (PostProcessComp && LUT)
	{
		PostProcessComp->Settings.ColorGradingLUT = LUT;
	}

	if (VolumetricCloud && CloudMaterial)
	{
		VolumetricCloud->SetMaterial(CloudMaterial);
	}

	if (SkySphere && SkySphereMesh)
	{
		SkySphere->SetStaticMesh(SkySphereMesh);
	}
	if (SkySphere && SkyMaterial)
	{
		SkySphere->SetMaterial(0, SkyMaterial);
	}

	// Ensure Niagara components don't crash if system is null
	if (RainFX && NS_RainFall)
	{
		RainFX->SetAsset(NS_RainFall);
	}
	if (SnowFX && NS_SnowFall)
	{
		SnowFX->SetAsset(NS_SnowFall);
	}

	// Audio assets left null by default -> asignar en editor
}

// BeginPlay
void AWeatherSystemMaster::BeginPlay()
{
	Super::BeginPlay();

	// Timers: solo inicializamos si tiene sentido; el usuario puede ajustar tiempo en editor
	if (WeatherRandomTime > 0.0f)
	{
		// usamos WeatherRandomTime como intervalo; no asumimos loop: usa WeatherRandomTimeLoop si se desea
		GetWorldTimerManager().SetTimer(RandomWeatherTimerHandle, this, &AWeatherSystemMaster::WeatherRandomFunc, WeatherRandomTime, WeatherRandomTimeLoop);
	}

	if (ThunderWaitingTime > 0.0f)
	{
		GetWorldTimerManager().SetTimer(ThunderTimerHandle, this, &AWeatherSystemMaster::ThunderAudioPlay, ThunderWaitingTime, true);
	}

	// Iniciar SunTimeLine si existe y tiene curve
	if (SunTimeLine && SunAngleCurve)
	{
		// Bind dinámico: solo si la curva existe
		UpdateSunFloat.BindDynamic(this, &AWeatherSystemMaster::UpdateSunTimeLine);
		SunTimeLine->AddInterpFloat(SunAngleCurve, UpdateSunFloat);
		SunTimeLine->PlayFromStart();
	}

	// IsNight timeline
	if (IsNightTimeLine && IsNightCurve)
	{
		UpdateIsNightFloat.BindDynamic(this, &AWeatherSystemMaster::UpdateIsNightTimeLine);
		IsNightTimeLine->AddInterpFloat(IsNightCurve, UpdateIsNightFloat);
	}

	// Otros timelines: bind solo si curves existen
	if (SnowOrRainFallingTimeLine && SnowOrRainFallingCurve)
	{
		UpdateSnowOrRainFallingFloat.BindDynamic(this, &AWeatherSystemMaster::UpdateSnowOrRainFallingTimeLine);
		SnowOrRainFallingTimeLine->AddInterpFloat(SnowOrRainFallingCurve, UpdateSnowOrRainFallingFloat);
	}

	if (RainFallingTimeLine && RainFallingCurve)
	{
		UpdateRainFallingFloat.BindDynamic(this, &AWeatherSystemMaster::UpdateRainFallingTimeLine);
		RainFallingTimeLine->AddInterpFloat(RainFallingCurve, UpdateRainFallingFloat);
	}

	if (SnowFallingTimeLine && SnowFallingCurve)
	{
		UpdateSnowFallingFloat.BindDynamic(this, &AWeatherSystemMaster::UpdateSnowFallingTimeLine);
		SnowFallingTimeLine->AddInterpFloat(SnowFallingCurve, UpdateSnowFallingFloat);
	}

	if (GetWetTimeLine && GetWetCurve)
	{
		UpdateGetWetFloat.BindDynamic(this, &AWeatherSystemMaster::UpdateGetWetTimeLine);
		GetWetTimeLine->AddInterpFloat(GetWetCurve, UpdateGetWetFloat);
	}

	if (FrozenTimeLine && FrozenCurve)
	{
		UpdateFrozenFloat.BindDynamic(this, &AWeatherSystemMaster::UpdateFrozenTimeLine);
		FrozenTimeLine->AddInterpFloat(FrozenCurve, UpdateFrozenFloat);
	}

	// Seguridad: desactivar audio que pueda quedar en reproducción por accidente
	if (RainAudio) RainAudio->Stop();
	if (SnowAudio) SnowAudio->Stop();
	if (ThunderAudio) ThunderAudio->Stop();
	if (WindAudio) WindAudio->Stop();
	if (BirdsAudio) BirdsAudio->Stop();
	if (CicadaAudio) CicadaAudio->Stop();
}

// Tick
void AWeatherSystemMaster::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Comportamiento básico: si hay precipitación, seguir al jugador para FX
	if ((bSnowIsComing || bRainIsComing))
	{
		// GetPlayerLocation implementada en .h; comprobamos seguridad
		if (UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
		{
			GetPlayerLocation();
		}
	}

	// TODO: actualizaciones periódicas (timelines, efectos) las iremos rellenando aquí
}

// WeatherRandomFunc: placeholder seguro
void AWeatherSystemMaster::WeatherRandomFunc()
{
	// Implementación detallada posterior.
	// Por ahora, función segura que no hace nada si no hay condiciones.
	// Puedes copiar la lógica que tenías antes cuando quieras activarla.
}

// GetPlayerLocation: mueve los FX al jugador (implementación segura)
void AWeatherSystemMaster::GetPlayerLocation()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn) return;

	FVector PlayerLocation = PlayerPawn->GetActorLocation() + FVector(0.0f, 0.0f, 500.0f);

	if (SnowFX) SnowFX->SetWorldLocation(PlayerLocation);
	if (RainFX) RainFX->SetWorldLocation(PlayerLocation);
}

// ThunderAudioPlay: placeholder seguro
void AWeatherSystemMaster::ThunderAudioPlay()
{
	// Reproduce si está lloviendo (ejemplo)
	if (bRainIsComing)
	{
		if (ThunderAudio && !ThunderAudio->IsPlaying())
		{
			ThunderAudio->Play();
			float Volume = FMath::FRandRange(ThunderMinVolume, ThunderMaxVolume);
			ThunderAudio->SetVolumeMultiplier(Volume);
		}
	}
	else
	{
		if (ThunderAudio && ThunderAudio->IsPlaying())
		{
			ThunderAudio->Stop();
		}
	}
}

// Timeline callbacks: implementaciones mínimas (seguros)
void AWeatherSystemMaster::UpdateIsNightTimeLine(float IsNightOutput)
{
	// Ajuste de intensity de Skylight (seguro: comprueba existencia)
	if (SkyLight)
	{
		SkyLight->SetIntensity(FMath::Lerp(SkyLightIntensityOfDay, SkyLightIntensityOfNight, IsNightOutput));
	}
	// Más ajustes se añadirán aquí (MPC, audios, etc.)
}

void AWeatherSystemMaster::UpdateSunTimeLine(float SunOutput)
{
	// Ajusta rotación del sol de forma segura
	if (DirectionalLight)
	{
		FRotator NewRot = FRotator(SunOutput, 0.0f, 0.0f);
		DirectionalLight->SetRelativeRotation(NewRot);
	}
}

void AWeatherSystemMaster::UpdateSnowOrRainFallingTimeLine(float SnowOrRainFallingOutput)
{
	// Ejemplo: variar intensidad de luz y fog de forma segura
	if (DirectionalLight)
	{
		float NewIntensity = FMath::Lerp(SunIntensityOfSunny, SunIntensityOfRainOrSnow, SnowOrRainFallingOutput);
		DirectionalLight->SetIntensity(NewIntensity);
	}
	if (ExponentialHeightFog)
	{
		float NewFog = FMath::Lerp(FogDensityOfSunny, FogDensityOfRainOrSnow, SnowOrRainFallingOutput);
		ExponentialHeightFog->SetFogDensity(NewFog);
	}
}

void AWeatherSystemMaster::UpdateGetWetTimeLine(float GetWetOutput)
{
	// Placeholder: manipular MPC / materiales más tarde
}

void AWeatherSystemMaster::UpdateFrozenTimeLine(float FrozenOutput)
{
	// Placeholder: manipular MPC / congelación más tarde
}

void AWeatherSystemMaster::UpdateRainFallingTimeLine(float RainFallingOutput)
{
	// Ejemplo: variar spawnrate de niagara si está asignado (seguro)
	if (RainFX)
	{
		// Se asume que el sistema tiene parámetros llamados "SpawnRate" etc.
		RainFX->SetFloatParameter(FName(TEXT("SpawnRate")), FMath::Lerp(0.0f, MaxRainFalling, RainFallingOutput));
	}
	if (RainAudio)
	{
		RainAudio->SetVolumeMultiplier(FMath::Lerp(0.0f, 1.0f, RainFallingOutput));
	}
}

void AWeatherSystemMaster::UpdateSnowFallingTimeLine(float SnowFallingOutput)
{
	if (SnowFX)
	{
		SnowFX->SetFloatParameter(FName(TEXT("SnowNumber")), FMath::Lerp(0.0f, MaxSnowFalling, SnowFallingOutput));
	}
	if (SnowAudio)
	{
		SnowAudio->SetVolumeMultiplier(FMath::Lerp(0.0f, 1.0f, SnowFallingOutput));
	}
}