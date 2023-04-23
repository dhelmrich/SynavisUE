// Fill out your copyright notice in the Description page of Project Settings.


#include "CropFieldSpawner.h"


// includes for the scene generation, including all objects that are spawnable
// Light sources
#include "Engine/PointLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SpotLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
// Meshes
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
// Materials and Runtime Textures
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureSampleParameterCube.h"
#include "Materials/MaterialExpressionTextureSampleParameterSubUV.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureSampleParameter2DArray.h"
// Visual Components
#include "Engine/ExponentialHeightFog.h"
#include "Components/DecalComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
// Audio



// static variable to contain the classes by their names
static const TMap<FString, UClass*> ClassMap
{
  {"PointLight", APointLight::StaticClass()},
  {"DirectionalLight", ADirectionalLight::StaticClass()},
    {"SpotLight", ASpotLight::StaticClass()},
    {"RectLight", ARectLight::StaticClass()},
    {"SkyLight", ASkyLight::StaticClass()},
    {"ProceduralMeshComponent", UProceduralMeshComponent::StaticClass()},
    {"BoxComponent", UBoxComponent::StaticClass()},
    {"MaterialInstanceDynamic", UMaterialInstanceDynamic::StaticClass()},
    {"Material", UMaterial::StaticClass()},
    {"MaterialExpressionTextureSample", UMaterialExpressionTextureSample::StaticClass()},
    {"MaterialExpressionTextureSampleParameter2D", UMaterialExpressionTextureSampleParameter2D::StaticClass()},
    {"MaterialExpressionTextureSampleParameterCube", UMaterialExpressionTextureSampleParameterCube::StaticClass()},
    {"MaterialExpressionTextureSampleParameterSubUV", UMaterialExpressionTextureSampleParameterSubUV::StaticClass()},
    {"MaterialExpressionTextureSampleParameter", UMaterialExpressionTextureSampleParameter::StaticClass()},
    {"MaterialExpressionTextureObject", UMaterialExpressionTextureObject::StaticClass()},
    {"MaterialExpressionTextureSampleParameter2DArray", UMaterialExpressionTextureSampleParameter2DArray::StaticClass()},
    {"ExponentialHeightFog", AExponentialHeightFog::StaticClass()},
    {"DecalComponent", UDecalComponent::StaticClass()},
    {"SceneCaptureComponent2D", USceneCaptureComponent2D::StaticClass()},
    {"VolumetricCloudComponent", UVolumetricCloudComponent::StaticClass()},
    {"PostProcessComponent", UPostProcessComponent::StaticClass()}
};

// static variable to contain the spawn parameters associated with class names
static const TMap<FString, TArray<TPair<FString, FString>>> SpawnParameters
{
   {"PointLight", {
      {"Position","FVector"},
       {"LightColor", "FLinearColor"},
       {"Intensity", "float"},
       {"AttenuationRadius", "float"},
       {"SourceRadius", "float"},
       {"SoftSourceRadius", "float"},
       {"bUseInverseSquaredFalloff", "bool"},
       {"bAffectsWorld", "bool"},
       {"bEnableLightShaftBloom", "bool"},
       {"IndirectLightingIntensity", "float"},
       {"LightFunctionScale", "float"},
       {"LightFunctionFadeDistance", "float"},
       {"LightFunctionDisabledBrightness", "float"},
       {"bEnableLightShaftOcclusion", "bool"},
       {"bUseRayTracedDistanceFieldShadows", "bool"},
       {"bUseRayTracedDistanceFieldShadowsForStationaryLights", "bool"},
       {"bUseAreaShadowsForStationaryLights", "bool"},
       {"bUseRayTracedCascadeShadows", "bool"},
       {"bUseRayTracedCascadesForDynamicObjects", "bool"},
       {"bUseRayTracedCascadesForStationaryObjects", "bool"},
       {"bCastShadowsFromCinematicObjectsOnly", "bool"},
    }},
  {"DirectionalLight",{
      {"LightColor","FLinearColor"},
      {"Intensity","float"},
      {"bAffectsWorld","bool"},
      {"bUsedAsAtmosphereSunLight","bool"},
      {"bEnableLightShaftBloom","bool"},
      {"IndirectLightingIntensity","float"},
      {"LightFunctionScale","float"},
      {"LightFunctionFadeDistance","float"},
      {"LightFunctionDisabledBrightness","float"},
      {"bEnableLightShaftOcclusion","bool"},
    {"Orientation","FVector"}
  }},
  {"SpotLight",{
       {"LightColor","FLinearColor"},
      {"Intensity","float"},
      {"AttenuationRadius","float"},
      {"SourceRadius","float"},
      {"SoftSourceRadius","float"},
      {"bUseInverseSquaredFalloff","bool"},
      {"bAffectsWorld","bool"},
      {"bEnableLightShaftBloom","bool"},
      {"IndirectLightingIntensity","float"},
      {"LightFunctionScale","float"},
      {"LightFunctionFadeDistance","float"},
      {"LightFunctionDisabledBrightness","float"},
      {"bEnableLightShaftOcclusion","bool"},
      {"bUseRayTracedDistanceFieldShadows","bool"},
      {"bUseRayTracedDistanceFieldShadowsForStationaryLights","bool"},
      {"bUseAreaShadowsForStationaryLights","bool"},
      {"bUseRayTracedCascadeShadows","bool"},
      {"bUseRayTracedCascadesForDynamicObjects","bool"},
      {"bUseRayTracedCascadesForStationaryObjects","bool"},
      {"bCastShadowsFromCinematicObjectsOnly","bool"},
      {"bCastModulatedShadows","bool"},
      {"bCastFarShadow","bool"},
      {"bCastInsetShadow","bool"},
      {"bCastVolumetricShadow","bool"},
      {"bCastDynamicShadow","bool"},
      {"bCastStaticShadow","bool"},
      {"bCastCinematicShadow","bool"},
      {"bCastHiddenShadow","bool"},
      {"bCastShadowAsTwoSided","bool"},
      {"bCastFarShadow","bool"},
      {"bCastInsetShadow","bool"},
      {"bCastVolumetricShadow","bool"},
      {"bCastDynamicShadow","bool"},
      {"bCastStaticShadow","bool"},
      {"bCastCinematicShadow","bool"},
      {"bCastHiddenShadow","bool"},
      {"bCastShadowAsTwoSided","bool"},
      {"bCastFarShadow","bool"},
      {"bCastInsetShadow","bool"},
      {"bCastVolumetricShadow","bool"},
      {"bCastDynamicShadow","bool"},
      {"bCastStaticShadow","bool"},
      {"bCastCinematicShadow","bool"},
      {"bCastHiddenShadow","bool"},
      {"bCastShadowAsTwoSided","bool"},
      {"bCastFarShadow","bool"},
      {"bCastInsetShadow","bool"}
  }},
  {"SkyLight",{
        {"LightColor","FLinearColor"},
    {"Intensity","float"},
     {"IndirectLightingIntensity","float"},
     {"VolumetricScatterIntensity","float"},
    {"bCloudAmbientOcclusion","bool"},
  }},
  {"SkyAthmosphere",{
      {"GroundRadius","float"},
    {"GroundAlbedo","FLinearColor"},
    {"AtmosphereHeight","float"},
    {"MultiScattering","float"},
          {"RayleighScattering","float"},
    {"RayleighExponentialDistribution","float"},
    {"MieScattering","float"},
    {"MieExponentialDistribution","float"},
    {"MieAbsorption","float"},
    {"MieAnisotropy","float"},
    {"MiePhaseFunctionG","float"},
    {"MieScatteringDistribution","float"},
    {"MieAbsorptionDistribution","float"},
    {"MieAnisotropyDistribution","float"},
    {"MiePhaseFunctionGDistribution","float"},
    {"MieScatteringScale","float"},
    {"MieAbsorptionScale","float"},
    {"MieAnisotropyScale","float"},
    {"MiePhaseFunctionGScale","float"},
    {"MieScatteringExponentialDistribution","float"},
    {"AbsorptionScale","float"},
    {"Absorption","FLinearColor"}
  }},
  {"Mesh",{
    {"Position","FVector"},
        {"Rotation","FRotator"},
    {"Scale","FVector"},
    {"Mesh","FString"} // we use an FString here because there is a defined library of meshes
    // anything else is being provided by the user
  }}
};

// Sets default values
ACropFieldSpawner::ACropFieldSpawner()
{
  // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
  PrimaryActorTick.bCanEverTick = true;
  CropField = CreateDefaultSubobject<UBoxComponent>(TEXT("Space Bounds"));
}

void ACropFieldSpawner::Spawn(TArray<FVector> Points, TArray<FVector> Normals, TArray<int> Triangles,
  TArray<float> Scalars, float Min, float Max)
{
  AActor* Actor = GetWorld()->SpawnActor<AActor>();
  UProceduralMeshComponent* mesh = NewObject<UProceduralMeshComponent>(this);
  Actor->AddInstanceComponent(mesh);
  mesh->RegisterComponent();
}


UClass* ACropFieldSpawner::GetClassFromName(FString ClassName)
{
  UClass* Class = ClassMap.FindRef(ClassName);
  if (Class == nullptr)
  {
    UE_LOG(LogTemp, Error, TEXT("Class %s not found"), *ClassName);
  }
  return Class;
}

UTexture2D* ACropFieldSpawner::CreateTexture2DFromData(TArray<uint8> Data, int Width, int Height)
{
  UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height);
  Texture->UpdateResource();
  FTexture2DMipMap& Mip = Texture->PlatformData->Mips[0];
  void* DataPtr = Mip.BulkData.Lock(LOCK_READ_WRITE);
  FMemory::Memcpy(DataPtr, Data.GetData(), Data.Num());
  Mip.BulkData.Unlock();
  Texture->UpdateResource();
  return Texture;
}

// Called when the game starts or when spawned
void ACropFieldSpawner::BeginPlay()
{
  Super::BeginPlay();

}

// Called every frame
void ACropFieldSpawner::Tick(float DeltaTime)
{
  Super::Tick(DeltaTime);
  for (USceneComponent* component : SubComponents)
  {
    // retrieve the last time the component was rendered


  }
}

