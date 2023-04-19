// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"


#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Containers/Map.h"
#include "PixelStreamingInputComponent.h"

#include "GenericPlatform/GenericPlatformProcess.h"

#include "SynavisDrone.generated.h"

// callback definition for blueprints
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPixelStreamingResponseCallback, FString, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPixelStreamingDataCallback, TArray<int>, Data);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams( FPixelStreamingReceptionCallback, const TArray<FVector>&, Points, const TArray<FVector>&, Normals, const TArray<int>&, Triangles, const TArray<FVector2D>&, TexCoords, const TArray<float>&, Values, float, Time);

// forward

class UCameraComponent;
class USceneCaptureComponent2D;
class UBoxComponent;
class UTextureRenderTarget2D;


UENUM(BlueprintType)
enum class EGeometryReceptionState : uint8
{
  None = 0,
	Init,
	Points,
	Normals,
	Triangles,
	TexCoords
};

UENUM(BlueprintType)
enum class EBlueprintSignalling : uint8
{
  SwitchToSceneCam = 0,
	SwitchToInfoCam,
	SwitchToBothCams,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBlueprintSignallingCallback, EBlueprintSignalling, Signal);

UCLASS(Config = Game)
class SYNAVISUE_API ASynavisDrone : public AActor
{
	GENERATED_BODY()
	
public:

  UPROPERTY(VisibleAnywhere)
	EGeometryReceptionState ReceptionState = EGeometryReceptionState::None;

  UFUNCTION(BlueprintCallable, Category = "Network")
  void ParseInput(FString Descriptor);
	// Sets default values for this actor's properties
	ASynavisDrone();

	UPROPERTY(BlueprintAssignable, Category = "Network")
	FPixelStreamingReceptionCallback OnPixelStreamingGeometry;

	UPROPERTY(BlueprintAssignable, Category = "Network")
	FPixelStreamingResponseCallback OnPixelStreamingResponse;

	UPROPERTY(BlueprintAssignable, Category = "Coupling")
	FBlueprintSignallingCallback OnBlueprintSignalling;

  UFUNCTION(BlueprintCallable, Category = "Network")
  void SendResponse(FString Message);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "View")
		USceneCaptureComponent2D* InfoCam;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "View")
		USceneCaptureComponent2D* SceneCam;

	UFUNCTION(BlueprintCallable, Category = "Actor")
	  void LoadFromJSON(FString JasonString = "");

	UFUNCTION(BlueprintCallable, Category = "Actor")
	  FString ListObjectPropertiesAsJSON(UObject* Object);

	
  void ApplyJSONToObject(UObject* Object, FJsonObject *JSON);

	
  UObject* GetObjectFromJSON(FJsonObject *JSON);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Actor")
		USceneComponent* CoordinateSource;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View")
	  UBoxComponent* Flyspace;
		
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "View")
	UTextureRenderTarget2D* InfoCamTarget;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "View")
	UTextureRenderTarget2D* SceneCamTarget;
		
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "View")
	  float MaxVelocity = 10.f;
		
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "View")
	  float DistanceToLandscape = -1.f;
		
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "View")
	  float TurnWeight = 0.8f;
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "View")
	  float CircleStrength = 0.02f;
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "View")
	  float CircleSpeed = 0.3f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Time")
	  float FrameCaptureTime = 10.f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "View")
	  int RenderMode = 2;
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "View")
	  float DistanceScale = 2000.f;
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "View")
	  float BlackDistance = 0.f;
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "View")
	  float DirectionalIntensity = 10.0f;
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "View")
	  float DirectionalIndirectIntensity = 0.485714f;
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "View")
	  float AmbientIntensity = 1.0f;
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "View")
	  float AmbientVolumeticScattering = 1.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Camera")
	  float FocalRate = 10.f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Camera")
	  float MaxFocus = 2000.f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Camera")
	  bool AdjustFocalDistance = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "View")
	  class UUserWidget* Overlay;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "View")
	  FVector BinScale{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View")
	  bool LockNavigation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View")
	  bool EditorOrientedCamera = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View")
		bool Rain = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "View")
	  int RainParticlesPerSecond{10000};

  UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Network")
    int ConfiguredPort = 50121;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Time")
	  float FlightProximityTriggerDistance = 10.f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Time")
	  bool PrintScreenNewPosition = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "Time")
	  float AutoExposureBias = 0.413f;

  UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category = "View")
		int MaxFPS = -1.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Network")
    UPixelStreamingInput* RemoteInput;

	UFUNCTION(BlueprintCallable, Category = "Network")
		const bool IsInEditor() const;
		
	FCriticalSection Mutex;
	bool CalculatedMaximumInOffThread = false;
	float LastComputedMaximum = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "View")
	  float CurrentDivider = 10000.f;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	FVector NextLocation;
	FVector Velocity;
	FVector SpaceOrigin;
	FVector SpaceExtend;
	float MeanVelocityLength = 0;
	uint64_t SampleSize = 0;
	UMaterial* PostProcessMat;
	float LowestLandscapeBound;
	class UMaterialInstanceDynamic* CallibratedPostprocess;

	float FocalLength;
	float TargetFocalLength;
  FCollisionObjectQueryParams ParamsObject;
  FCollisionQueryParams ParamsTrace;
	

	float xprogress = 0.f;
	float FrameCaptureCounter;


	FJsonObject JsonConfig;
	TArray<FVector> Points;
	TArray<FVector> Normals;
	TArray<int> Triangles;
	TArray<FVector2D> UVs;
	TArray<float> Scalar;
	unsigned int PointCount = 0;
	unsigned int TriangleCount = 0;

  FCollisionObjectQueryParams ActorFilter;
  FCollisionQueryParams CollisionFilter;
	void EnsureDistancePreservation();

	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};