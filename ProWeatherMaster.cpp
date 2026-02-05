// Copyright PixelForgeESP 2026. All rights reserved.

#include "ProWeatherMaster.h"
#include "Math/UnrealMathUtility.h"
#include "TimerManager.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

AProWeatherMaster::AProWeatherMaster(){
	PrimaryActorTick.bCanEverTick = true;
	SetupDefaults();
}

void AProWeatherMaster::BeginPlay(){
	Super::BeginPlay();

	// BIND TIMELINES FIRST
	UpdateSunFloat.BindDynamic(this, &AProWeatherMaster::UpdateSunTimeLine);
	if (SunAngleCurve) { SunTimeLine->AddInterpFloat(SunAngleCurve, UpdateSunFloat); }

	UpdateIsNightFloat.BindDynamic(this, &AProWeatherMaster::UpdateIsNightTimeLine);
	if (IsNightCurve) { IsNightTimeLine->AddInterpFloat(IsNightCurve, UpdateIsNightFloat); }

	UpdateSnowOrRainFallingFloat.BindDynamic(this, &AProWeatherMaster::UpdateSnowOrRainFallingTimeLine);
	if (SnowOrRainFallingCurve) { SnowOrRainFallingTimeLine->AddInterpFloat(SnowOrRainFallingCurve, UpdateSnowOrRainFallingFloat); }

	UpdateRainFallingFloat.BindDynamic(this, &AProWeatherMaster::UpdateRainFallingTimeLine);
	if (RainFallingCurve) { RainFallingTimeLine->AddInterpFloat(RainFallingCurve, UpdateRainFallingFloat); }

	UpdateSnowFallingFloat.BindDynamic(this, &AProWeatherMaster::UpdateSnowFallingTimeLine);
	if (SnowFallingCurve) { SnowFallingTimeLine->AddInterpFloat(SnowFallingCurve, UpdateSnowFallingFloat); }

	// SKY TIME MODE
	switch (SkyTimeMode) {
	case ESkyTimeMode::Dynamic:
		SunTimeLine->Play();
		break;

	case ESkyTimeMode::Day:
		UpdateSunTimeLine(233.0f);
		UpdateIsNightTimeLine(0.0f);
		break;

	case ESkyTimeMode::Night:
		UpdateSunTimeLine(100.0f);
		UpdateIsNightTimeLine(1.0f);
		break;
	}

	// WEATHER INIT
	if (bEnableDynamicWeather) {
		WeatherRandomFunc();

		GetWorldTimerManager().SetTimer(
			RandomWeatherTimerHandle,
			this,
			&AProWeatherMaster::WeatherRandomFunc,
			WeatherRandomTime,
			true
		);
	} else {
		SetWeatherState(CurrentWeatherState);
	}

	// THUNDER TIMER
	GetWorldTimerManager().SetTimer(ThunderTimerHandle,this,&AProWeatherMaster::ThunderAudioPlay,ThunderWaitingTime,true);
}

void AProWeatherMaster::Tick(float DeltaTime){
	Super::Tick(DeltaTime);

	// LLAMAR EFECTOS
	if (!bSnowIsComing && !bRainIsComing) { return; }

	// ACTUALIZAR FX CADA 0.2S (5 VECES POR SEGUNDO)
	FXUpdateTimer += DeltaTime;
	if (FXUpdateTimer >= 0.2f) {
		FXUpdateTimer = 0.0f;
		GetPlayerLocation();
	}
}

void AProWeatherMaster::WeatherRandomFunc(){
	if (!bEnableDynamicWeather) { return; }

	const float Roll = FMath::FRand();

	if (Roll < SnowProbability) {
		SetWeatherState(EWeatherState::Snow);
	} else if (Roll < SnowProbability + RainProbability) {
		SetWeatherState(EWeatherState::Rain);
	} else {
		SetWeatherState(EWeatherState::Sunny);
	}
}

void AProWeatherMaster::SetWeatherState(EWeatherState NewState)
{
	const bool bSameState = (CurrentWeatherState == NewState);
	CurrentWeatherState = NewState;

	// RESET BASE
	bSnowIsComing = false;
	bRainIsComing = false;

	if (!SnowOrRainFallingTimeLine) { return; }

	if (RainFallingTimeLine) { RainFallingTimeLine->Reverse(); }
	if (SnowFallingTimeLine) { SnowFallingTimeLine->Reverse(); }

	DirectionalLight->SetEnableLightShaftBloom(true);
	DirectionalLight->SetEnableLightShaftOcclusion(true);

	// SELECCION DE ESTADO
	switch (NewState)
	{
	case EWeatherState::Snow:
		bSnowIsComing = true;
		SnowOrRainFallingTimeLine->PlayFromStart();
		if (SnowFallingTimeLine) { SnowFallingTimeLine->PlayFromStart(); }
		break;
	case EWeatherState::Rain:
		bRainIsComing = true;
		SnowOrRainFallingTimeLine->PlayFromStart();
		if (RainFallingTimeLine) { RainFallingTimeLine->PlayFromStart(); }
		break;
	case EWeatherState::Sunny:
	default:
		break;
	}

	// MODIFICAR VALORES DE LA LUZ DIRECCIONAL
	if (bSnowIsComing || bRainIsComing) {
		DirectionalLight->SetEnableLightShaftBloom(false);
		DirectionalLight->SetEnableLightShaftOcclusion(false);
	}
}

// FUNCIÓN DE LA NOCHE
void AProWeatherMaster::UpdateIsNightTimeLine(float IsNightOutput)
{
	const bool bNowNight = (IsNightOutput >= 0.5f);
	bIsNight = bNowNight;

	// MODIFICACIÓN DE COMPONENTES
	if (SkyLight) {
		SkyLight->SetIntensity(
			FMath::Lerp(SkyLightIntensityDay, SkyLightIntensityNight, IsNightOutput)
		);
	}

	if (DirectionalLight) {
		DirectionalLight->SetIntensity(
			FMath::Lerp(SunIntensityDay, SunIntensityNight, IsNightOutput)
		);
	}

	if (MPC) {
		UKismetMaterialLibrary::SetVectorParameterValue(
			this, MPC, TEXT("SnowColor"),
			FMath::Lerp(SnowColorDay, SnowColorNight, IsNightOutput)
		);

		UKismetMaterialLibrary::SetVectorParameterValue(
			this, MPC, TEXT("RainColor"),
			FMath::Lerp(RainColorDay, RainColorNight, IsNightOutput)
		);
	}

	const bool bBadWeather = (bSnowIsComing || bRainIsComing);

	// CAMBIAR PARAMETROS DE CIELO
	if (MPC) {
		const float StarsValue = bBadWeather
			? 0.0f
			: FMath::Lerp(0.0f, 3.0f, IsNightOutput);

		UKismetMaterialLibrary::SetScalarParameterValue(
			this, MPC, TEXT("StarsMask"), StarsValue
		);

		const float CloudValue = bBadWeather
			? 0.5f
			: FMath::Lerp(0.5f, 1.0f, IsNightOutput);

		UKismetMaterialLibrary::SetScalarParameterValue(
			this, MPC, TEXT("CloudDisappear"), CloudValue
		);
	}

	// DESACTIVAR AUDIOS DE FAUNA
	if (BirdsAudio) {
		BirdsAudio->SetVolumeMultiplier(
			(bBadWeather || bNowNight) ? 0.0f : CachedBirdsDayVolume
		);
	}

	if (CicadaAudio) {
		CicadaAudio->SetVolumeMultiplier(
			(bBadWeather || bNowNight) ? 0.0f : CachedCicadaDayVolume
		);
	}
}

// FUNCIÓN DE ROTACIÓN DE LA LUZ DIRECCIONAL
void AProWeatherMaster::UpdateSunTimeLine(float SunOutput)
{
	if (!DirectionalLight) { return; }

	switch (SkyTimeMode) {
	case ESkyTimeMode::Day:
	{
		// SOL FIJO DE DÍA
		DirectionalLight->SetRelativeRotation(FRotator(233.0f, 0.0f, 0.0f));

		bIsNight = false;
		IsNightTimeLine->Reverse();
		break;
	}

	case ESkyTimeMode::Night:
	{
		// SOL FIJO DE NOCHE
		DirectionalLight->SetRelativeRotation(FRotator(100.0f, 0.0f, 0.0f));

		bIsNight = true;
		IsNightTimeLine->Play();
		break;
	}

	case ESkyTimeMode::Dynamic:
	default:
	{
		// ROTACION DINÁMICA DEL SOL
		DirectionalLight->SetRelativeRotation(FRotator(SunOutput, 0.0f, 0.0f));

		// LOOP SEGURO DEL TIMELINE
		if (SunOutput >= 360.0f) { SunTimeLine->PlayFromStart(); }

		// RANGO NOCTURNO
		const bool bShouldBeNight = (SunOutput >= 2.5f && SunOutput <= 167.5f);

		if (bShouldBeNight && !bIsNight) {
			bIsNight = true;
			IsNightTimeLine->Play();
		}
		else if (!bShouldBeNight && bIsNight) {
			bIsNight = false;
			IsNightTimeLine->Reverse();
		}
		break;
	}
	}
}

// OBTENER LOCALIZACIÓN DEL JUGADOR
void AProWeatherMaster::GetPlayerLocation()
{
	APawn* DefaultPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!DefaultPawn) { return; }

	const FVector PlayerLocation = DefaultPawn->GetActorLocation() + FVector(0.f, 0.f, FXHeightOffset);

	// OPTIMIZACIÓN POR MOVIMIENTO
	if (FVector::DistSquared(LastPlayerLocation, PlayerLocation) < 10000.f) { return; }
	LastPlayerLocation = PlayerLocation;

	// DETECTAR TECHO
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(DefaultPawn);

	const bool bUnderRoof = GetWorld()->LineTraceSingleByChannel(
		Hit,
		DefaultPawn->GetActorLocation(),
		DefaultPawn->GetActorLocation() + FVector(0.f, 0.f, 2000.f),
		ECC_Visibility,
		Params
	);

	// ACTIVAR / DESACTIVAR FX
	if (bUnderRoof) {
		if (RainFX) RainFX->Deactivate();
		if (SnowFX) SnowFX->Deactivate();

		// ATENUAR AUDIO BAJO TECHO
		if (RainAudio) { RainAudio->SetVolumeMultiplier(-1.0f); }
		if (SnowAudio) { SnowAudio->SetVolumeMultiplier(-1.0f); }
		if (ThunderAudio) { ThunderAudio->SetVolumeMultiplier(-0.75f); }

		return;
	}

	// RESTAURAR AUDIO AL AIRE LIBRE
	if (RainAudio) { RainAudio->SetVolumeMultiplier(1.0f); }
	if (ThunderAudio) { ThunderAudio->SetVolumeMultiplier(1.0f); }

	// POSICIONAR FX
	if (RainFX) {
		RainFX->SetWorldLocation(PlayerLocation);
		RainFX->Activate();
	}

	if (SnowFX) {
		SnowFX->SetWorldLocation(PlayerLocation);
		SnowFX->Activate();
	}
}

// DECIDIR SI PUEDE SONAR UN TRUENO
void AProWeatherMaster::ThunderAudioPlay()
{
	// SI AMBOS COMPONENTES EXISTEN
	if (!ThunderAudio || !ThunderSound) { return; }

	// SOLO REPRODUCE SI ESTA LLOVIENDO Y EVITA SONAR CUANDO NO
	if (!bRainIsComing) {
		if (ThunderAudio->IsPlaying()) { ThunderAudio->Stop(); }
		GetWorldTimerManager().ClearTimer(ThunderTimerHandle);
		return;
	}

	// EVITA SOLAPAMIENTO DE TRUENOS
	if (ThunderAudio->IsPlaying()) { return; }

	// VARIAR SONIDO DE CADA TRUENO
	const float RandomVolume = FMath::RandRange(ThunderMinVolume, ThunderMaxVolume);
	ThunderAudio->SetVolumeMultiplier(RandomVolume);

	// SALIDA DE SONIDOS
	ThunderAudio->SetSound(ThunderSound);
	ThunderAudio->Play();

	// PROGRAMAR SIGUIENTE TRUENO
	ScheduleNextThunder();
}

// PROGRAMACION DEL SIGUIENTE TRUENO
void AProWeatherMaster::ScheduleNextThunder()
{
	// SI SIGUE LLOVIENDO
	if (!bRainIsComing) { return; }

	// PREPARA EL SIGUIENTE TRUENO EN UN RANGO DE TIEMPO
	const float NextThunderTime = FMath::RandRange(ThunderWaitingTime * 0.5f, ThunderWaitingTime * 1.5f);

	// CREAR TIMER NO REPETITIVO
	GetWorldTimerManager().SetTimer(
		ThunderTimerHandle,
		this,
		&AProWeatherMaster::ThunderAudioPlay,
		NextThunderTime,
		false
	);
}

// PREPARA LOS COMPONENTES PARA LA LLUVIA O NIEVE
void AProWeatherMaster::UpdateSnowOrRainFallingTimeLine(float SnowOrRainFallingOutput)
{
	// EVITA CRASHES SI ALGUNO NO EXISTE
	if (!DirectionalLight || !ExponentialHeightFog || !SkyAtmosphere) { return; }

	// PLAY WIND AUDIO
	if (WindAudio) { WindAudio->SetVolumeMultiplier(FMath::Lerp(0.0f, CachedWindVolume, SnowOrRainFallingOutput)); }

	// MODIFICAR COMPONENTES
	DirectionalLight->SetIntensity(FMath::Lerp(SunIntensitySunny, SunIntensityRainOrSnow, SnowOrRainFallingOutput));
	DirectionalLight->SetTemperature(FMath::Lerp(SunTemperatureDay, SunTemperatureRainOrSnow, SnowOrRainFallingOutput));

	ExponentialHeightFog->SetFogDensity(FMath::Lerp(FogDensitySunny, FogDensityRainOrSnow, SnowOrRainFallingOutput));
	ExponentialHeightFog->SetFogHeightFalloff(FMath::Lerp(FogHeightFalloffSunny, FogHeightFalloffRainOrSnow, SnowOrRainFallingOutput));
	ExponentialHeightFog->SetStartDistance(FMath::Lerp(DistanceDay, DistanceRainOrSnow, SnowOrRainFallingOutput));

	SkyAtmosphere->SetSkyLuminanceFactor(FMath::Lerp(SnowColorDay, DarkClouds, SnowOrRainFallingOutput));

	// MODIFICAR LAS NUBES Y DESACTIVAR ESTRELLAS
	if (MPC) {
		UKismetMaterialLibrary::SetVectorParameterValue(
			this, MPC, TEXT("CloudColor"),
			FMath::Lerp(SnowColorDay, DarkClouds, SnowOrRainFallingOutput)
		);
		UKismetMaterialLibrary::SetScalarParameterValue(
			this, MPC, TEXT("StarsMask"), 0.0f
		);
	}

	// AUDIO AMBIENTE DIURNO
	if (!bIsNight) {
		if (BirdsAudio) { BirdsAudio->SetVolumeMultiplier(FMath::Lerp(CachedBirdsVolume, 0.0f, SnowOrRainFallingOutput)); }
		if (CicadaAudio) { CicadaAudio->SetVolumeMultiplier(FMath::Lerp(CachedCicadaVolume, 0.0f, SnowOrRainFallingOutput)); }
	}
}

// TIMELINE PARA LA LLUVIA
void AProWeatherMaster::UpdateRainFallingTimeLine(float RainFallingOutput)
{
	// GENERAR EFECTOS
	if (RainFX) {
		RainFX->SetFloatParameter(TEXT("SpawnRate"), FMath::Lerp(0.0f, MaxRainFalling, RainFallingOutput));
		RainFX->SetFloatParameter(TEXT("SpawnRate_02"), FMath::Lerp(0.0f, MaxRainFalling * 0.01f, RainFallingOutput));
		RainFX->SetFloatParameter(TEXT("SpawnRate_Fog"), FMath::Lerp(0.0f, MaxRainFogFalling, RainFallingOutput));
	}

	// REPRODUCIR AUDIO
	if (RainAudio) { RainAudio->SetVolumeMultiplier(FMath::Lerp(0.0f, 1.0f, RainFallingOutput)); }
}

// TIMELINE PARA LA NIEVE
void AProWeatherMaster::UpdateSnowFallingTimeLine(float SnowFallingOutput)
{
	// GENERAR EFECTO
	if (SnowFX) {
		SnowFX->SetFloatParameter(
			TEXT("SnowNumber"),
			FMath::Lerp(0.0f, MaxSnowFalling, SnowFallingOutput)
		);
	}

	// REPRODUCIR AUDIO
	if (SnowAudio) { SnowAudio->SetVolumeMultiplier(FMath::Lerp(0.0f, 1.0f, SnowFallingOutput)); }}

void AProWeatherMaster::SetupDefaults(){
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	AProWeatherMaster::SetRootComponent(RootComponent);

	DirectionalLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("DirectionalLight"));
	DirectionalLight->SetupAttachment(RootComponent);
	DirectionalLight->SetMobility(EComponentMobility::Movable);
	DirectionalLight->SetRelativeRotation_Direct(FRotator(233.0f, 0.0f, 0.0f));
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

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(RootComponent);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->Intensity = 1.2f;
	SkyLight->bRealTimeCapture = true;
	SkyLight->bCloudAmbientOcclusion = true;

	ExponentialHeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("ExponentialHeightFog"));
	ExponentialHeightFog->SetupAttachment(RootComponent);
	ExponentialHeightFog->SetMobility(EComponentMobility::Movable);
	ExponentialHeightFog->SetRelativeLocation(FVector(0.0f, 0.0f, 5000.0f));

	PostProcessComp = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcessComp->SetupAttachment(RootComponent);
	PostProcessComp->SetMobility(EComponentMobility::Movable);
	PostProcessComp->bUnbound = true;
	PostProcessComp->Settings.bOverride_BloomIntensity = true;
	PostProcessComp->Settings.BloomIntensity = 2.0f;
	PostProcessComp->Settings.bOverride_AutoExposureBias = true;
	PostProcessComp->Settings.AutoExposureBias = 0.0f;
	PostProcessComp->Settings.bOverride_VignetteIntensity = true;
	PostProcessComp->Settings.VignetteIntensity = 0.2f;

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(RootComponent);
	SkyAtmosphere->SetMobility(EComponentMobility::Movable);
	SkyAtmosphere->SetSkyLuminanceFactor(FVector(0.5f, 0.625f, 1.0f));

	VolumetricCloud = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("VolumetricCloud"));
	VolumetricCloud->SetupAttachment(RootComponent);
	VolumetricCloud->SetMobility(EComponentMobility::Movable);
	VolumetricCloud->LayerBottomAltitude = 20.0f;
	CloudMaterial = LoadObject<UMaterialInstance>(NULL, TEXT("MaterialInstanceConstant'/ProWeather/Instancials/Sky/MI_SimpleVolumetricCloud_1.MI_SimpleVolumetricCloud_1'"));
	VolumetricCloud->SetMaterial(CloudMaterial);
	VolumetricCloud->bUsePerSampleAtmosphericLightTransmittance = true;

	SkySphere = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkySphere"));
	SkySphere->SetupAttachment(RootComponent);
	SkySphereMesh = LoadObject<UStaticMesh>(NULL, TEXT("StaticMesh'/ProWeather/Meshes/Sky/SM_SkySphere.SM_SkySphere'"));
	SkySphere->SetStaticMesh(SkySphereMesh);
	SkySphere->SetRelativeScale3D(FVector(5000.0f));
	SkyMaterial = LoadObject<UMaterial>(NULL, TEXT("MaterialInstanceConstant'/ProWeather/Instancials/Sky/MI_Sky_1.MI_Sky_1'"));
	SkySphere->SetMaterial(0, SkyMaterial);
	SkySphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkySphere->CastShadow = false;

	RainFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RainFX"));
	RainFX->SetupAttachment(RootComponent);
	RainFX->SetVisibility(true);
	NS_RainFall = LoadObject<UNiagaraSystem>(NULL, TEXT("NiagaraSystem'/ProWeather/Particles/NS_RainFall.NS_RainFall'"));
	RainFX->SetAsset(NS_RainFall);

	RainAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RainAudio"));
	RainAudio->SetupAttachment(RootComponent);
	RainSound = LoadObject<USoundCue>(NULL, TEXT("SoundCue'/ProWeather/Music/Cue/Cue_Rain.Cue_Rain'"));
	RainAudio->SetSound(RainSound);
	RainAudio->VolumeMultiplier = 0.0f;

	ThunderAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ThunderAudio"));
	ThunderAudio->SetupAttachment(RootComponent);
	ThunderSound = LoadObject<USoundCue>(NULL, TEXT("SoundCue'/ProWeather/Music/Cue/Cue_Thunder.Cue_Thunder'"));
	ThunderAudio->SetSound(ThunderSound);
	ThunderAudio->VolumeMultiplier = 0.0f;

	SnowFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SnowFX"));
	SnowFX->SetupAttachment(RootComponent);
	SnowFX->SetVisibility(true);
	NS_SnowFall = LoadObject<UNiagaraSystem>(NULL, TEXT("NiagaraSystem'/ProWeather/Particles/NS_SnowFall.NS_SnowFall'"));
	SnowFX->SetAsset(NS_SnowFall);

	SnowAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("SnowAudio"));
	SnowAudio->SetupAttachment(RootComponent);
	SnowSound = LoadObject<USoundCue>(NULL, TEXT("SoundCue'/ProWeather/Music/Cue/Cue_Snow.Cue_Snow'"));
	SnowAudio->SetSound(SnowSound);
	SnowAudio->VolumeMultiplier = 0.0f;

	WindAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("WindAudio"));
	WindAudio->SetupAttachment(RootComponent);
	WindSound = LoadObject<USoundCue>(NULL, TEXT("SoundCue'/ProWeather/Music/Cue/Cue_Wind.Cue_Wind'"));
	WindAudio->SetSound(WindSound);
	WindAudio->VolumeMultiplier = 0.0f;

	BirdsAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BirdsAudio"));
	BirdsAudio->SetupAttachment(RootComponent);
	BirdsSound = LoadObject<USoundCue>(NULL, TEXT("SoundCue'/ProWeather/Music/Cue/Cue_Birds.Cue_Birds'"));
	BirdsAudio->SetSound(BirdsSound);

	CicadaAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("CicadaAudio"));
	CicadaAudio->SetupAttachment(RootComponent);
	CicadaSound = LoadObject<USoundCue>(NULL, TEXT("SoundCue'/ProWeather/Music/Cue/Cue_Cicada_Loop.Cue_Cicada_Loop'"));
	CicadaAudio->SetSound(CicadaSound);

	IsNightTimeLine = CreateDefaultSubobject<UTimelineComponent>(TEXT("IsNightTimeLine"));
	IsNightCurve = LoadObject<UCurveFloat>(NULL, TEXT("CurveFloat'/ProWeather/Curves/IsNightLerp.IsNightLerp'"));

	SunTimeLine = CreateDefaultSubobject<UTimelineComponent>(TEXT("SunTimeLine"));
	SunAngleCurve = LoadObject<UCurveFloat>(NULL, TEXT("CurveFloat'/ProWeather/Curves/SunAngle.SunAngle'"));

	SnowOrRainFallingTimeLine = CreateDefaultSubobject<UTimelineComponent>(TEXT("SnowOrRainFallingTimeLine"));
	SnowOrRainFallingCurve = LoadObject<UCurveFloat>(NULL, TEXT("CurveFloat'/ProWeather/Curves/SnowOrRainFalling.SnowOrRainFalling'"));

	RainFallingTimeLine = CreateDefaultSubobject<UTimelineComponent>(TEXT("RainFallingTimeLine"));
	RainFallingCurve = LoadObject<UCurveFloat>(NULL, TEXT("CurveFloat'/ProWeather/Curves/RainFalling.RainFalling'"));

	SnowFallingTimeLine = CreateDefaultSubobject<UTimelineComponent>(TEXT("SnowFallingTimeLine"));
	SnowFallingCurve = LoadObject<UCurveFloat>(NULL, TEXT("CurveFloat'/ProWeather/Curves/SnowFalling.SnowFalling'"));

	MPC = LoadObject<UMaterialParameterCollection>(NULL, TEXT("MaterialParameterCollection'/ProWeather/MPC/MPC_Weather.MPC_Weather'"));

	SnowColorDay = FLinearColor(0.5f, 0.65f, 1.0f, 1.0f);
	SnowColorNight = FLinearColor(0.25f, 0.325f, 0.5f, 1.0f);
	DarkClouds = FLinearColor(0.035f, 0.0395f, 0.05f, 1.0f);
	RainColorDay = SnowColorDay;
	RainColorNight = SnowColorNight;
}
