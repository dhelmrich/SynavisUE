// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldSpawner.h"
#include "SynavisDrone.h"

// Asset Registry
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

// includes for the scene generation, including all objects that are spawnable
// Light sources
#include "Engine/PointLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SpotLight.h"
#include "Engine/RectLight.h"
#include "Engine/SkyLight.h"
// Meshes
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
// Audio


// static variable to contain the classes by their names
static const TMap<FString, UClass*> ClassMap
{
  {TEXT("PointLight"),                                            APointLight::StaticClass()},
  {TEXT("DirectionalLight"),                                      ADirectionalLight::StaticClass()},
  {TEXT("SpotLight"),                                             ASpotLight::StaticClass()},
  {TEXT("RectLight"),                                             ARectLight::StaticClass()},
  {TEXT("SkyLight"),                                              ASkyLight::StaticClass()},
  {TEXT("ProceduralMeshComponent"),                               UProceduralMeshComponent::StaticClass()},
  {TEXT("BoxComponent"),                                          UBoxComponent::StaticClass()},
  {TEXT("MaterialInstanceDynamic"),                               UMaterialInstanceDynamic::StaticClass()},
  {TEXT("Material"),                                              UMaterial::StaticClass()},
  {TEXT("MaterialExpressionTextureSample"),                       UMaterialExpressionTextureSample::StaticClass()},
  {TEXT("MaterialExpressionTextureSampleParameter2D"),            UMaterialExpressionTextureSampleParameter2D::StaticClass()},
  {TEXT("MaterialExpressionTextureSampleParameterCube"),          UMaterialExpressionTextureSampleParameterCube::StaticClass()},
  {TEXT("MaterialExpressionTextureSampleParameterSubUV"),         UMaterialExpressionTextureSampleParameterSubUV::StaticClass()},
  {TEXT("MaterialExpressionTextureSampleParameter"),              UMaterialExpressionTextureSampleParameter::StaticClass()},
  {TEXT("MaterialExpressionTextureObject"),                       UMaterialExpressionTextureObject::StaticClass()},
  {TEXT("MaterialExpressionTextureSampleParameter2DArray"),       UMaterialExpressionTextureSampleParameter2DArray::StaticClass()},
  {TEXT("ExponentialHeightFog"),                                  AExponentialHeightFog::StaticClass()},
  {TEXT("DecalComponent"),                                        UDecalComponent::StaticClass()},
  {TEXT("SceneCaptureComponent2D"),                               USceneCaptureComponent2D::StaticClass()},
  {TEXT("VolumetricCloudComponent"),                              UVolumetricCloudComponent::StaticClass()},
  {TEXT("PostProcessComponent"),                                  UPostProcessComponent::StaticClass()}
};

// static variable to contain the spawn parameters associated with class names
// shortcut properties are: Position, Rotation, Scale, Visibility
static const TMap<FString, TArray<TPair<FString, FString>>> SpawnParameters
{
   {TEXT("PointLight"), {
      {(TEXT("Position")),(TEXT("FVector"))},
      {(TEXT("LightColor")), (TEXT("FLinearColor"))},
      {(TEXT("Intensity")), (TEXT("float"))},
      {(TEXT("AttenuationRadius")), (TEXT("float"))},
      { TEXT("SourceRadius"),  TEXT("float")},
      { TEXT("SoftSourceRadius"),  TEXT("float")},
      { TEXT("bUseInverseSquaredFalloff"),  TEXT("bool")},
      { TEXT("bEnableLightShaftBloom"),  TEXT("bool")},
      { TEXT("IndirectLightingIntensity"),  TEXT("float")},
      { TEXT("LightFunctionScale"),  TEXT("float")},
      { TEXT("LightFunctionFadeDistance"),  TEXT("float")},
      { TEXT("LightFunctionDisabledBrightness"),  TEXT("float")},
      { TEXT("bEnableLightShaftOcclusion"),  TEXT("bool")},
    }},
  { TEXT("DirectionalLight"),{
    { TEXT("LightColor"), TEXT("FLinearColor")},
    { TEXT("Intensity"), TEXT("float")},
    { TEXT("bAffectsWorld"), TEXT("bool")},
    { TEXT("bUsedAsAtmosphereSunLight"), TEXT("bool")},
    { TEXT("bEnableLightShaftBloom"), TEXT("bool")},
    { TEXT("IndirectLightingIntensity"), TEXT("float")},
    { TEXT("LightFunctionScale"), TEXT("float")},
    { TEXT("LightFunctionFadeDistance"), TEXT("float")},
    { TEXT("LightFunctionDisabledBrightness"), TEXT("float")},
    { TEXT("bEnableLightShaftOcclusion"), TEXT("bool")},
    { TEXT("Orientation"), TEXT("FVector")}
  }},
  { TEXT("SpotLight"),{
    { TEXT("LightColor"), TEXT("FLinearColor")},
    { TEXT("Intensity"), TEXT("float")},
    { TEXT("AttenuationRadius"), TEXT("float")},
    { TEXT("SourceRadius"), TEXT("float")},
    { TEXT("SoftSourceRadius"), TEXT("float")},
    { TEXT("bUseInverseSquaredFalloff"), TEXT("bool")},
    { TEXT("bEnableLightShaftBloom"), TEXT("bool")},
    { TEXT("IndirectLightingIntensity"), TEXT("float")},
    { TEXT("LightFunctionScale"), TEXT("float")},
    { TEXT("LightFunctionFadeDistance"), TEXT("float")},
    { TEXT("LightFunctionDisabledBrightness"), TEXT("float")},
    { TEXT("bEnableLightShaftOcclusion"), TEXT("bool")}
  }},
  { TEXT("SkyLight"),{
    { TEXT("LightColor"), TEXT("FLinearColor")},
    { TEXT("Intensity"), TEXT("float")},
    { TEXT("IndirectLightingIntensity"), TEXT("float")},
    { TEXT("VolumetricScatterIntensity"), TEXT("float")},
    { TEXT("bCloudAmbientOcclusion"), TEXT("bool")},
  }},
  { TEXT("SkyAthmosphere"),{
    { TEXT("GroundRadius"), TEXT("float")},
    { TEXT("GroundAlbedo"), TEXT("FLinearColor")},
    { TEXT("AtmosphereHeight"), TEXT("float")},
    { TEXT("MultiScattering"), TEXT("float")},
    { TEXT("RayleighScattering"), TEXT("float")},
    { TEXT("RayleighExponentialDistribution"), TEXT("float")},
    { TEXT("MieScattering"), TEXT("float")},
    { TEXT("MieExponentialDistribution"), TEXT("float")},
    { TEXT("MieAbsorption"), TEXT("float")},
    { TEXT("MieAnisotropy"), TEXT("float")},
    { TEXT("MiePhaseFunctionG"), TEXT("float")},
    { TEXT("MieScatteringDistribution"), TEXT("float")},
    { TEXT("MieAbsorptionDistribution"), TEXT("float")},
    { TEXT("MieAnisotropyDistribution"), TEXT("float")},
    { TEXT("MiePhaseFunctionGDistribution"), TEXT("float")},
    { TEXT("MieScatteringScale"), TEXT("float")},
    { TEXT("MieAbsorptionScale"), TEXT("float")},
    { TEXT("MieAnisotropyScale"), TEXT("float")},
    { TEXT("MiePhaseFunctionGScale"), TEXT("float")},
    { TEXT("MieScatteringExponentialDistribution"), TEXT("float")},
    { TEXT("AbsorptionScale"), TEXT("float")},
    { TEXT("Absorption"), TEXT("FLinearColor")}
  }},
  { TEXT("Mesh"),{
    { TEXT("Position"), TEXT("FVector")},
    { TEXT("Rotation"), TEXT("FRotator")},
    { TEXT("Scale"), TEXT("FVector")},
    { TEXT("Mesh"), TEXT("FString")} // we use an FString here because there is a defined library of meshes
    // anything else is being provided by the user
  }},
  { TEXT("Texture"),{
    { TEXT("Texture"), TEXT("Binary")}, // binary is not a real type, but signposts that we perform a binary read
    { TEXT("Size"), TEXT("FVector2D")},
    { TEXT("Material"), TEXT("FString")} // we use an FString here because there is a defined library of materials
  }},
  { TEXT("BoxComponent"),{
    { TEXT("Position"), TEXT("FVector")},
    { TEXT("Orientation"), TEXT("FVector")},
    { TEXT("Scale"), TEXT("FVector")}
  }}
};

static bool HasAllParameters(const FString& Type, const FJsonObject Parameters)
{
  if (SpawnParameters.Contains(Type))
  {
    for (auto& Parameter : SpawnParameters[Type])
    {
      if (!Parameters.HasField(Parameter.Key))
      {
        return false;
      }
    }
    return true;
  }
  return false;
}

// Sets default values
AWorldSpawner::AWorldSpawner()
{
  // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
  PrimaryActorTick.bCanEverTick = true;
  CropField = CreateDefaultSubobject<UBoxComponent>(TEXT("Space Bounds"));

  // Retrieve list of assets
  // >5.1 version of this code adapted from https://kantandev.com/articles/finding-all-classes-blueprints-with-a-given-base
  FAssetRegistryModule* AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry& AssetRegistry = AssetRegistryModule->Get();
  TArray<FString> ContentPaths;
  ContentPaths.Add(TEXT("/Game"));
  ContentPaths.Add(TEXT("/Synavis"));
  AssetRegistry.ScanPathsSynchronous(ContentPaths);
  AssetCache = MakeShareable(new FJsonObject());
  TSet<FTopLevelAssetPath> MeshAssetPaths;
  TSet<FTopLevelAssetPath> MaterialAssetPaths;
  FARFilter Filter;
  Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
  Filter.bRecursiveClasses = true;
  Filter.PackagePaths.Add(*ContentPaths[0]);
  Filter.bRecursivePaths = true;
  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);
  for (auto& Asset : AssetList)
  {
    TSharedPtr<FJsonObject> AssetObject = MakeShareable(new FJsonObject());
    AssetObject->SetStringField("Package", Asset.PackagePath.ToString());
    AssetObject->SetStringField("Type", Asset.AssetClassPath.ToString());
    AssetObject->SetStringField("SearchType", "Mesh");
    AssetCache->SetObjectField(Asset.AssetName.ToString(), AssetObject);
  }
  Filter.ClassPaths.Empty();
  Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
  TArray<FAssetData> MaterialList;
  AssetRegistry.GetAssets(Filter, MaterialList);
  for (auto& Asset : MaterialList)
  {
    TSharedPtr<FJsonObject> AssetObject = MakeShareable(new FJsonObject());
    AssetObject->SetStringField("Package", Asset.PackagePath.ToString());
    AssetObject->SetStringField("Type", Asset.AssetClassPath.ToString());
    AssetObject->SetStringField("SearchType", "Material");
    AssetCache->SetObjectField(Asset.AssetName.ToString(), AssetObject);
  }
}

void AWorldSpawner::SpawnProcMesh(TArray<FVector> Points, TArray<FVector> Normals, TArray<int> Triangles,
  TArray<float> Scalars, float Min, float Max, TArray<FVector2D> TexCoords, TArray<FProcMeshTangent> Tangents)
{
  AActor* Actor = GetWorld()->SpawnActor<AActor>();
  UProceduralMeshComponent* mesh = NewObject<UProceduralMeshComponent>(this);
  Actor->AddInstanceComponent(mesh);
  mesh->RegisterComponent();
  mesh->CreateMeshSection_LinearColor(0, Points, Triangles, Normals, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
}


TArray<FString> AWorldSpawner::GetNamesOfSpawnableTypes()
{
  TArray<FString> Names;
  TArray<FString> Result;

  int pos = AssetCache->Values.GetKeys(Result);
  UE_LOG(LogInit, Log, TEXT("We have %d values in the Asset Cache"), pos);
  int pos2 = ClassMap.GetKeys(Names);
  UE_LOG(LogInit, Log, TEXT("We have %d values in the Class Map"), (pos2 - pos));
  Result.Append(Names);
  if (Result.Num() == 21)
  {
    UE_LOG(LogInit, Log, TEXT("We have %d values in the Result"), Result.Num());
  }
  return Result;
}

void AWorldSpawner::ReceiveStreamingCommunicatorRef()
{
  decltype(StreamableHandles) HandlesToRelease;
  // Go through present streaming handles
  for(auto & StreamingHandle : StreamableHandles)
  {
    // If the handle is valid
    if(StreamingHandle.IsValid() && StreamingHandle->HasLoadCompleted())
    {
      // spawn the object
      auto asset = StreamingHandle->GetLoadedAsset();
      GetWorld()->SpawnActor<AActor>(asset->GetClass());
      MessageToClient(FString::Printf(TEXT("{\"type\":\"spawned\",\"name\":\"%s\"}"), *asset->GetName()));
      HandlesToRelease.Add(StreamingHandle);
    }
  }
  StreamableHandles.RemoveAll([&HandlesToRelease](const TSharedPtr<FStreamableHandle>& Handle)
  {
     return HandlesToRelease.Contains(Handle);
  });
}


UClass* AWorldSpawner::GetClassFromName(FString ClassName)
{
  UClass* Class = ClassMap.FindRef(ClassName);
  if (Class == nullptr)
  {
    UE_LOG(LogTemp, Error, TEXT("Class %s not found"), *ClassName);
  }
  return Class;
}

UTexture2D* AWorldSpawner::CreateTexture2DFromData(TArray<uint8> Data, int Width, int Height)
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

UDynamicMaterialInstance* AWorldSpawner::GenerateInstanceFromName(FString InstanceName)
{
  return nullptr;
}

FString AWorldSpawner::SpawnObject(TSharedPtr<FJsonObject> Description)
{
  FString ObjectName;
  if (Description->TryGetStringField("object", ObjectName))
  {
    // check if the object is in the asset cache
    if (AssetCache->HasField(ObjectName))
    {
      // load the asset
      FSoftObjectPath AssetPath = FSoftObjectPath(AssetCache->GetObjectField(ObjectName)->GetStringField("Package") + "/" + ObjectName);
      FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
      auto data = Streamable.RequestAsyncLoad(AssetPath, FStreamableDelegate::CreateUObject(this, &AWorldSpawner::ReceiveStreamingCommunicatorRef));
      StreamableHandles.Add(data);
    }
    else if (ClassMap.Contains(ObjectName))
    {
      // check if the object is in the class map
      UClass* Class = ClassMap.FindRef(ObjectName);
      if (Class == nullptr)
      {
        UE_LOG(LogTemp, Error, TEXT("Class %s not found"), *ObjectName);
        return "";
      }
      AActor* Actor = GetWorld()->SpawnActor<AActor>(Class);
      if (Actor == nullptr)
      {
        UE_LOG(LogTemp, Error, TEXT("Failed to spawn actor of class %s"), *ObjectName);
        return "";
      }
      if(Description->HasField("parameters"))
      {
        auto parameters = Description->GetObjectField("parameters");
      }
      return Actor->GetName();
    }
    else
    {
      UE_LOG(LogTemp, Error, TEXT("Object %s not found"), *ObjectName);
    }
  }
  return "";
}

void AWorldSpawner::ReceiveStreamingCommunicatorRef(ASynavisDrone* inDroneRef)
{
  DroneRef = inDroneRef;
}

// Called when the game starts or when spawned
void AWorldSpawner::BeginPlay()
{
  Super::BeginPlay();

}

void AWorldSpawner::MessageToClient(FString Message)
{
  if (IsValid(DroneRef))
  {
    DroneRef->SendResponse(Message);
  }
}

// Called every frame
void AWorldSpawner::Tick(float DeltaTime)
{
  Super::Tick(DeltaTime);
  for (USceneComponent* component : SubComponents)
  {
    // retrieve the last time the component was rendered


  }
}

