// Fill out your copyright notice in the Description page of Project Settings.


#include "SynavisDrone.h"

#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Engine/Scene.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Components/BoxComponent.h"
#include "DSP/PassiveFilter.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Blueprint/UserWidget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Async/Async.h"
#include "InstancedFoliageActor.h"
#include "Landscape.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Engine/LevelStreaming.h"
#include "Serialization/JsonReader.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"

inline float GetColor(const FColor& c, uint32 i)
{
  switch (i)
  {
  default:
  case 0:
    return c.R;
  case 1:
    return c.G;
  case 2:
    return c.B;
  case 3:
    return c.A;
  }
}

void ASynavisDrone::ParseInput(FString Descriptor)
{
  // reinterpret the message as ASCII
  const auto* Data = reinterpret_cast<const char*>(*Descriptor);
  // parse into FString
  FString Message(UTF8_TO_TCHAR(Data));
  // remove line breaks
  Message.ReplaceInline(TEXT("\r"), TEXT(""));
  Message.ReplaceInline(TEXT("\n"), TEXT(""));

  UE_LOG(LogTemp, Warning, TEXT("M: %s"), *Message);
  if (Descriptor[0] == '{' && Descriptor[Descriptor.Len() - 1] == '}')
  {
    TSharedPtr<FJsonObject> Jason = MakeShareable(new FJsonObject());
    TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Descriptor);
    FJsonSerializer::Deserialize(Reader, Jason);

    UE_LOG(LogTemp, Warning, TEXT("Received %s"), *Descriptor);
    // check if the message is a geometry message
    if (Jason->HasField("type"))
    {
      auto type = Jason->GetStringField("type");
      if (type == "geometry")
      {
        if (Jason->TryGetNumberField("points", PointCount))
        {
          UE_LOG(LogTemp, Warning, TEXT("Preparing to receive %d Points"), PointCount);
          Points.Empty();
          Points.SetNumUninitialized(PointCount, true);
          Normals.Empty();
          Normals.SetNumUninitialized(PointCount, true);
        }
        if (Jason->TryGetNumberField("triangles", TriangleCount))
        {
          UE_LOG(LogTemp, Warning, TEXT("Preparing to receive %d Triangles"), TriangleCount);
          Triangles.Empty();
          Triangles.SetNumUninitialized(TriangleCount, true);
        }
      }
      else if (type == "direct")
      {
        FBase64 Base64;
        // this is the direct transmission of the geometry
        // this means that the properties contain the buffers
        Points.Empty();
        Normals.Empty();
        Triangles.Empty();
        FString id;
        // check which geometry this message is for
        if (Jason->HasField("id"))
        {
          id = Jason->GetStringField("id");
        }
        // fetch the geometry from the world

        // pre-allocate the data destination
        TArray<uint8> Dest;
        // determine the maximum size of the data
        uint64_t MaxSize = 0;
        for (auto Field : Jason->Values)
        {
          if (Field.Key == "type")
            continue;
          auto& Value = Field.Value;
          if (Value->Type == EJson::String)
          {
            auto Source = Value->AsString();
            if (Base64.GetDecodedDataSize(Source) > MaxSize)
              MaxSize = Source.Len();
          }
        }
        // allocate the destination buffer
        Dest.SetNumUninitialized(MaxSize);

        // get the json property for the points
        auto points = Jason->GetStringField("points");
        // decode the base64 string
        Base64.Decode(points, Dest);
        // copy the data into the points array
        Points.SetNumUninitialized(Dest.Num() / sizeof(FVector), true);
        FMemory::Memcpy(Points.GetData(), Dest.GetData(), Dest.Num());
        auto normals = Jason->GetStringField("normals");
        Dest.Reset();
        Base64.Decode(normals, Dest);
        Normals.SetNumUninitialized(Dest.Num() / sizeof(FVector), true);
        FMemory::Memcpy(Normals.GetData(), Dest.GetData(), Dest.Num());
        auto triangles = Jason->GetStringField("triangles");
        Dest.Reset();
        Base64.Decode(triangles, Dest);
        Triangles.SetNumUninitialized(Dest.Num() / sizeof(int), true);
        FMemory::Memcpy(Triangles.GetData(), Dest.GetData(), Dest.Num());
        UVs.Reset();
        Dest.Reset();
        auto uvs = Jason->GetStringField("texcoords");
        Base64.Decode(uvs, Dest);
        UVs.SetNumUninitialized(Dest.Num() / sizeof(FVector2D), true);
        FMemory::Memcpy(UVs.GetData(), Dest.GetData(), Dest.Num());
      }
      else if (type == "parameter")
      {
        auto* Target = this->GetObjectFromJSON(Jason.Get());
        ApplyJSONToObject(Target, Jason.Get());
      }
      else if (type == "query")
      {
        if (!Jason->HasField("object"))
        {
          // respond with names of all actors
          FString message = TEXT("{\"type\":\"query\",\"name\":\"all\",\"data\":[");
          TArray<AActor*> Actors;
          UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), Actors);

          for (auto i = 0; i < Actors.Num(); ++i)
          {
            message += FString::Printf(TEXT("\"%s\""), *Actors[i]->GetName());
            if (i < Actors.Num() - 1)
              message += TEXT(",");
          }
          message += TEXT("]}");
          this->SendResponse(message);
        }
        else if(Jason->HasField("property"))
        {
          auto* Target = this->GetObjectFromJSON(Jason.Get());
          if (Target != nullptr)
          {
            FString Name = Target->GetName();
            FString Property = Jason->GetStringField("property");
            FString JsonData = GetJSONFromObjectProperty(Target, Property);
            FString message = FString::Printf(TEXT("{\"type\":\"query\",\"name\":\"%s\",\"data\":%s}"), *Name, *JsonData);
            this->SendResponse(message);
          }
        }
        else
        {
          auto* Target = this->GetObjectFromJSON(Jason.Get());
          if (Target != nullptr)
          {
            FString Name = Target->GetName();
            FString JsonData = ListObjectPropertiesAsJSON(Target);
            FString message = FString::Printf(TEXT("{\"type\":\"query\",\"name\":\"%s\",\"data\":%s}"), *Name, *JsonData);
            this->SendResponse(message);
          }
        }
      }
      else if (type == "command")
      {
        // received a command
        FString Name = Jason->GetStringField("name");
        if (Name == "reset")
        {
          // reset the geometry
          Points.Empty();
          Normals.Empty();
          Triangles.Empty();
          UVs.Empty();
        }
      }

      else if (type == "info")
      {
        if (Jason->HasField("frametime"))
        {
          const FString Response = FString::Printf(TEXT("{\"type\":\"info\",\"frametime\":%f}"), GetWorld()->GetDeltaSeconds());
          SendResponse(Response);
        }
        else if (Jason->HasField("memory"))
        {
          const FString Response = FString::Printf(TEXT("{\"type\":\"info\",\"memory\":%d}"), FPlatformMemory::GetStats().TotalPhysical);
          SendResponse(Response);
        }
        else if (Jason->HasField("fps"))
        {
          const FString Response = FString::Printf(TEXT("{\"type\":\"info\",\"fps\":%d}"), static_cast<uint32_t>(FPlatformTime::ToMilliseconds(FPlatformTime::Cycles64())));
          SendResponse(Response);
        }
        else if (Jason->HasField("object"))
        {
          FString RequestedObjectName = Jason->GetStringField("object");
          TArray<AActor*> FoundActors;

        }
      }
      else if (type == "console")
      {
        if (Jason->HasField("command"))
        {
          FString Command = Jason->GetStringField("command");
          UE_LOG(LogTemp, Warning, TEXT("Console command %s"), *Command);
          auto* Controller = GetWorld()->GetFirstPlayerController();
          if (Controller)
          {
            Controller->ConsoleCommand(Command);
          }
        }
      }
      else if (type == "settings")
      {
        // check for settings subobject and put it into member
        if (Jason->HasField("settings"))
        {
          auto Settings = Jason->GetObjectField("settings");
          LoadFromJSON(Message);
        }
      }
    }
  }
  else
  {
    switch (ReceptionState)
    {
    case EGeometryReceptionState::None:
      ReceptionState = EGeometryReceptionState::Init;
      break;
    case EGeometryReceptionState::Init:
      ReceptionState = EGeometryReceptionState::Points;
      break;
    case EGeometryReceptionState::Points:
      ReceptionState = EGeometryReceptionState::Normals;
      break;
    case EGeometryReceptionState::Normals:
      ReceptionState = EGeometryReceptionState::Triangles;
      break;
    case EGeometryReceptionState::Triangles:
      ReceptionState = EGeometryReceptionState::None;
      break;
    default:
      break;
    }
  }
}

void ASynavisDrone::SendResponse(FString Descriptor)
{
  FString Response(reinterpret_cast<TCHAR*>(TCHAR_TO_ANSI(*Descriptor)));
  UE_LOG(LogTemp, Warning, TEXT("Sending response: %s"), *Descriptor);
  OnPixelStreamingResponse.Broadcast(Response);
}

// Sets default values
ASynavisDrone::ASynavisDrone()
{
  // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
  PrimaryActorTick.bCanEverTick = true;

  CoordinateSource = CreateDefaultSubobject<USceneComponent>(TEXT("Root Component"));
  RootComponent = CoordinateSource;
  InfoCam = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Information Camera"));
  InfoCam->SetupAttachment(RootComponent);
  Flyspace = CreateDefaultSubobject<UBoxComponent>(TEXT("Fly Space"));
  Flyspace->SetBoxExtent({ 100.f,100.f,100.f });

  //Flyspace->SetupAttachment(RootComponent);

  //static ConstructorHelpers::FObjectFinder<UMaterial> Filter(TEXT("Material'/SynavisUE/SegmentationMaterial'"));
  static ConstructorHelpers::FObjectFinder<UMaterial> Filter(TEXT("Material'/SynavisUE/SteeringMaterial.SteeringMaterial'"));
  if (Filter.Succeeded())
  {
    PostProcessMat = Filter.Object;

  }

  static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> InfoTarget(TEXT("TextureRenderTarget2D'/SynavisUE/SceneTarget.SceneTarget'"));
  if (InfoTarget.Succeeded())
  {
    InfoCamTarget = InfoTarget.Object;
  }
  else
  {
    UE_LOG(LogTemp, Error, TEXT("Could not load one of the textures."));
  }

  static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> SceneTarget(TEXT("TextureRenderTarget2D'/SynavisUE/InfoTarget.InfoTarget'"));
  if (SceneTarget.Succeeded())
  {
    SceneCamTarget = SceneTarget.Object;
  }
  else
  {
    UE_LOG(LogTemp, Error, TEXT("Could not load one of the textures."));
  }

  SceneCam = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Rendering Camera"));
  SceneCam->SetupAttachment(RootComponent);
  InfoCam->SetRelativeLocation({ 0,0,0 });
  SceneCam->SetRelativeLocation({ 0, 0, 0 });

  ParamsObject.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);
  ParamsObject.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldStatic);
  ParamsTrace.AddIgnoredActor(this);


  LoadConfig();
  this->SetActorTickEnabled(false);
}

void ASynavisDrone::LoadFromJSON(FString JasonString)
{
  if (JasonString == "")
  {
    FString LevelName = GetLevel()->GetOuter()->GetName();
    FString File = FPaths::GeneratedConfigDir() + UGameplayStatics::GetPlatformName() + TEXT("/") + LevelName + TEXT(".json");
    UE_LOG(LogTemp, Warning, TEXT("Testing for file: %s"), *File);
    if (!FFileHelper::LoadFileToString(JasonString, *File))
    {
      FFileHelper::SaveStringToFile(TEXT("{}"), *File);
      return;
    }
  }
  TSharedPtr<FJsonObject> Jason = MakeShareable(new FJsonObject());
  TSharedRef<TJsonReader<TCHAR>> Reader = FJsonStringReader::Create(JasonString);
  FJsonSerializer::Deserialize(Reader, Jason);
  for (auto Key : Jason->Values)
  {
    if (Key.Key.StartsWith(TEXT("type")))
    {
      // skip network type
      continue;
    }
    auto* prop = GetClass()->FindPropertyByName(FName(*(Key.Key.RightChop(1))));
    if (Key.Key.StartsWith(TEXT("f")))
    {
      FNumericProperty* fprop = CastField<FNumericProperty>(prop);
      if (fprop)
      {
        fprop->SetFloatingPointPropertyValue(fprop->ContainerPtrToValuePtr<void>(this), Key.Value->AsNumber());
      }
    }
    else if (Key.Key.StartsWith(TEXT("i")))
    {
      FNumericProperty* iprop = CastField<FNumericProperty>(prop);
      if (iprop)
      {
        iprop->SetIntPropertyValue(iprop->ContainerPtrToValuePtr<void>(this), (int64)Key.Value->AsNumber());
      }
    }
    else if (Key.Key.StartsWith(TEXT("v")))
    {
      if (!LockNavigation && Key.Key.Contains(TEXT("Position")))
      {
        FVector loc(Key.Value->AsObject()->GetNumberField(TEXT("x")),
          Key.Value->AsObject()->GetNumberField(TEXT("y")),
          Key.Value->AsObject()->GetNumberField(TEXT("z")));
        Flyspace->SetWorldLocation(loc);
      }
      else if (!LockNavigation && Key.Key.Contains(TEXT("Orientation")))
      {
        FRotator rot(Key.Value->AsObject()->GetNumberField(TEXT("p")),
          Key.Value->AsObject()->GetNumberField(TEXT("y")),
          Key.Value->AsObject()->GetNumberField(TEXT("r")));
        InfoCam->SetRelativeRotation(rot);
        SceneCam->SetRelativeRotation(rot);
      }
      else
      {
        FStructProperty* vprop = CastField<FStructProperty>(prop);
        if (vprop)
        {
          TSharedPtr<FJsonObject> Values = Key.Value->AsObject();
          FVector* Output = vprop->ContainerPtrToValuePtr<FVector>(this);
          Output->X = Values->GetNumberField(TEXT("x"));
          Output->Y = Values->GetNumberField(TEXT("y"));
          Output->Z = Values->GetNumberField(TEXT("z"));
        }
      }
    }
    else if (Key.Key.StartsWith(TEXT("b")))
    {
      FBoolProperty* bprop = CastField<FBoolProperty>(prop);
      if (bprop)
      {
        bprop->SetPropertyValue(bprop->ContainerPtrToValuePtr<void>(this), Key.Value->AsBool());
      }
    }
  }
}

FString ASynavisDrone::ListObjectPropertiesAsJSON(UObject* Object)
{
  const UClass* Class = Object->GetClass();
  FString OutputString = TEXT("{");

  for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
  {
    FProperty* Property = *It;
    auto name = Property->GetName();
    auto type = Property->GetCPPType();
    OutputString += TEXT(" \"") + name + TEXT("\": \"") + type + TEXT("\"");
    // check if we are at the end of the list
    if (It->Next)
    {
      OutputString += TEXT(",");
    }
  }
  return OutputString + TEXT(" }");
}

void ASynavisDrone::ApplyJSONToObject(UObject* Object, FJsonObject* JSON)
{
  // received a parameter update
  FString Name = JSON->GetStringField("property");

  USceneComponent* ComponentIdentity = Cast<USceneComponent>(Object);
  AActor* ActorIdentity = Cast<AActor>(Object);
  auto* Property = Object->GetClass()->FindPropertyByName(*Name);
  if (ActorIdentity)
  {
    ComponentIdentity = ActorIdentity->GetRootComponent();
  }

  // Find out whether one of the shortcut properties was updated
  if (ComponentIdentity)
  {
    if (Name == "position")
    {
      if (JSON->HasField("x") && JSON->HasField("y") && JSON->HasField("z"))
      {
        ComponentIdentity->SetWorldLocation(FVector(JSON->GetNumberField("x"), JSON->GetNumberField("y"), JSON->GetNumberField("z")));
        return;
      }
    }
    else if (Name == "orientation")
    {
      if (JSON->HasField("p") && JSON->HasField("y") && JSON->HasField("r"))
      {
        ComponentIdentity->SetWorldRotation(FRotator(JSON->GetNumberField("p"), JSON->GetNumberField("y"), JSON->GetNumberField("r")));
        return;
      }
    }
    else if (Name == "scale")
    {
      if (JSON->HasField("x") && JSON->HasField("y") && JSON->HasField("z"))
      {
        ComponentIdentity->SetWorldScale3D(FVector(JSON->GetNumberField("x"), JSON->GetNumberField("y"), JSON->GetNumberField("z")));
        return;
      }
    }
    else if (Name == "visibility")
    {
      if (JSON->HasField("value"))
        ComponentIdentity->SetVisibility(JSON->GetBoolField("value"));
      return;
    }
  }
  if (Property)
  {
    if (Property->IsA(FIntProperty::StaticClass()))
    {
      auto* IntProperty = CastField<FIntProperty>(Property);
      IntProperty->SetPropertyValue_InContainer(Object, JSON->GetIntegerField("value"));
    }
    else if (Property->IsA(FFloatProperty::StaticClass()))
    {
      auto* FloatProperty = CastField<FFloatProperty>(Property);
      FloatProperty->SetPropertyValue_InContainer(Object, JSON->GetNumberField("value"));
    }
    else if (Property->IsA(FBoolProperty::StaticClass()))
    {
      auto* BoolProperty = CastField<FBoolProperty>(Property);
      BoolProperty->SetPropertyValue_InContainer(Object, JSON->GetBoolField("value"));
    }
    else if (Property->IsA(FStrProperty::StaticClass()))
    {
      auto* StringProperty = CastField<FStrProperty>(Property);
      StringProperty->SetPropertyValue_InContainer(Object, JSON->GetStringField("value"));
    }
    // check if property is a vector
    else if (Property->IsA(FStructProperty::StaticClass()))
    {
      auto* StructProperty = CastField<FStructProperty>(Property);
      // check if the struct is a vector via the JSON
      if (JSON->HasField("x") && JSON->HasField("y") && JSON->HasField("z"))
      {
        auto* VectorValue = StructProperty->ContainerPtrToValuePtr<FVector>(Object);
        if (VectorValue)
        {
          VectorValue->X = JSON->GetNumberField("x");
          VectorValue->Y = JSON->GetNumberField("y");
          VectorValue->Z = JSON->GetNumberField("z");
        }
      }
    }
  }
  else
  {
    UE_LOG(LogTemp, Warning, TEXT("Property %s not found"), *Name);
    SendResponse(TEXT("{\"type\":\"error\",\"message\":\"Property not found\"}"));
  }
}

UObject* ASynavisDrone::GetObjectFromJSON(FJsonObject* JSON)
{
  FString Name = JSON->GetStringField("object");
  // divide the name by dots
  TArray<FString> Parts;
  Name.ParseIntoArray(Parts, TEXT("."), true);

  // match the first part of the name to the actor
  TArray<AActor*> FoundActors;
  UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);
  for (auto* Actor : FoundActors)
  {
    if (Actor->GetName() == Parts[0])
    {
      // if there is only one part, return the actor
      if (Parts.Num() == 1)
      {
        return Actor;
      }
      // otherwise, remove part and go deeper, descending by component
      else
      {
        UActorComponent* comp = nullptr;
        Parts.RemoveAt(0);
        while (Parts.Num() > 0)
        {
          for (auto* Component : Actor->GetComponents())
          {
            if (Component->GetName() == Parts[0])
            {
              comp = Component;
              break;
            }
          }
          Parts.RemoveAt(0);
        }
        return comp;
      }
    }
  }
  return nullptr;
}

FString ASynavisDrone::GetJSONFromObjectProperty(UObject* Object, FString PropertyName)
{
  USceneComponent* ComponentIdentity = Cast<USceneComponent>(Object);
  AActor* ActorIdentity = Cast<AActor>(Object);
  auto* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
  if (ActorIdentity)
  {
    ComponentIdentity = ActorIdentity->GetRootComponent();
  }
  // Find out whether one of the shortcut properties was requested
  if (ComponentIdentity)
  {
    if (PropertyName == "position")
    {
      FVector Position = ComponentIdentity->GetComponentLocation();
      return FString::Printf(TEXT("{\"x\":%f,\"y\":%f,\"z\":%f}"), Position.X, Position.Y, Position.Z);
    }
    else if (PropertyName == "orientation")
    {
      FRotator Orientation = ComponentIdentity->GetComponentRotation();
      return FString::Printf(TEXT("{\"p\":%f,\"y\":%f,\"r\":%f}"), Orientation.Pitch, Orientation.Yaw, Orientation.Roll);
    }
    else if (PropertyName == "scale")
    {
      FVector Scale = ComponentIdentity->GetComponentScale();
      return FString::Printf(TEXT("{\"x\":%f,\"y\":%f,\"z\":%f}"), Scale.X, Scale.Y, Scale.Z);
    }
    else if (PropertyName == "visibility")
    {
      return FString::Printf(TEXT("{\"value\":%s}"), ComponentIdentity->IsVisible() ? TEXT("true") : TEXT("false"));
    }
  }
  if (Property)
  {
    // find out whether the property is a vector
    const auto StructProperty = CastField<FStructProperty>(Property);
    const auto FloatProperty = CastField<FFloatProperty>(Property);
    const auto IntProperty = CastField<FIntProperty>(Property);
    const auto BoolProperty = CastField<FBoolProperty>(Property);
    const auto StringProperty = CastField<FStrProperty>(Property);
    if (StructProperty)
    {
      // check if the struct is a vector via the JSON
      if (StructProperty->Struct->GetFName() == "Vector")
      {
        auto* VectorValue = StructProperty->ContainerPtrToValuePtr<FVector>(Object);
        if (VectorValue)
        {
          return FString::Printf(TEXT("{\"x\":%f,\"y\":%f,\"z\":%f}"), VectorValue->X, VectorValue->Y, VectorValue->Z);
        }
      }
    }
    else if (FloatProperty)
    {
      auto* FloatValue = FloatProperty->ContainerPtrToValuePtr<float>(Object);
      if (FloatValue)
      {
        return FString::Printf(TEXT("{\"value\":%f}"), *FloatValue);
      }
    }
    else if (IntProperty)
    {
      auto* IntValue = IntProperty->ContainerPtrToValuePtr<int>(Object);
      if (IntValue)
      {
        return FString::Printf(TEXT("{\"value\":%d}"), *IntValue);
      }
    }
    else if (BoolProperty)
    {
      auto* BoolValue = BoolProperty->ContainerPtrToValuePtr<bool>(Object);
      if (BoolValue)
      {
        return FString::Printf(TEXT("{\"value\":%s}"), *BoolValue ? TEXT("true") : TEXT("false"));
      }
    }
    else if (StringProperty)
    {
      auto* StringValue = StringProperty->ContainerPtrToValuePtr<FString>(Object);
      if (StringValue)
      {
        return FString::Printf(TEXT("{\"value\":\"%s\"}"), **StringValue);
      }
    }
    else
    {
      UE_LOG(LogTemp, Warning, TEXT("Property %s not vector, float, bool, or string"), *PropertyName);
      SendResponse(TEXT("{\"type\":\"error\",\"message\":\"Property not vector, float, bool, or string\"}"));
      return TEXT("{}");
    }
  }
  UE_LOG(LogTemp, Warning, TEXT("Property %s not found"), *PropertyName);
  SendResponse(TEXT("{\"type\":\"error\",\"message\":\"Property not found\"}"));
  return TEXT("{}");
}

  const bool ASynavisDrone::IsInEditor() const
  {
#ifdef WITH_EDITOR
    return true;
#else
    return false;
#endif
  }

  // Called when the game starts or when spawned
  void ASynavisDrone::BeginPlay()
  {
    this->SetActorTickEnabled(false);
    Super::BeginPlay();

    InfoCam->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
    SceneCam->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
    SpaceOrigin = Flyspace->GetComponentLocation();
    SpaceExtend = Flyspace->GetScaledBoxExtent();

    Flyspace->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    LoadFromJSON();
    auto* world = GetWorld();
    CallibratedPostprocess = UMaterialInstanceDynamic::Create(PostProcessMat, this);
    InfoCam->AddOrUpdateBlendable(CallibratedPostprocess, 1.f);
    CallibratedPostprocess->SetScalarParameterValue(TEXT("DistanceScale"), DistanceScale);
    CallibratedPostprocess->SetScalarParameterValue(TEXT("BlackDistance"), BlackDistance);
    CallibratedPostprocess->SetScalarParameterValue(TEXT("Mode"), (float)RenderMode);
    CallibratedPostprocess->SetVectorParameterValue(TEXT("BinScale"), (FLinearColor)BinScale);
    UE_LOG(LogTemp, Warning, TEXT("L:(%d,%d,%d) - E:(%d,%d,%d)"), SpaceOrigin.X, SpaceOrigin.Y, SpaceOrigin.Z, SpaceExtend.X, SpaceExtend.Y, SpaceExtend.Z);
    NextLocation = UKismetMathLibrary::RandomPointInBoundingBox(Flyspace->GetComponentLocation(), Flyspace->GetScaledBoxExtent());
    FrameCaptureCounter = FrameCaptureTime;

    SceneCam->PostProcessSettings.AutoExposureBias = this->AutoExposureBias;
    if (Rain)
    {
      ANiagaraActor* RainActor = Cast<ANiagaraActor>(UGameplayStatics::GetActorOfClass(world, ANiagaraActor::StaticClass()));
      if (RainActor)
      {
        RainActor->GetNiagaraComponent()->Activate(true);
        //RainActor->GetNiagaraComponent()->SetIntParameter(TEXT("SpawnRate"),RainParticlesPerSecond);
        RainActor->GetNiagaraComponent()->SetNiagaraVariableInt(TEXT("SpawnRate"), RainParticlesPerSecond);
      }
    }

    if (MaxFPS > 1)
    {
      APlayerController* PController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
      if (PController)
      {
        UE_LOG(LogTemp, Warning, TEXT("t.MaxFPS %d"), MaxFPS);
        PController->ConsoleCommand(*FString::Printf(TEXT("t.MaxFPS %d"), MaxFPS), true);
      }
    }

    TArray<AActor*> Found;
    TArray<USceneComponent*> PlantInstances;
    UGameplayStatics::GetAllActorsOfClass(world, APawn::StaticClass(), Found);
    CollisionFilter.AddIgnoredActors(Found);
    Found.Empty();

    auto* Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(GetWorld(),
      ADirectionalLight::StaticClass()));
    auto* Ambient = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(GetWorld(),
      ASkyLight::StaticClass()));
    auto* Clouds = Cast<AVolumetricCloud>(UGameplayStatics::GetActorOfClass(GetWorld(),
      AVolumetricCloud::StaticClass()));
    auto* Atmosphere = Cast<ASkyAtmosphere>(UGameplayStatics::GetActorOfClass(GetWorld(),
      ASkyAtmosphere::StaticClass()));

    if (Sun)
    {
      Sun->GetLightComponent()->SetIntensity(DirectionalIntensity);
      Sun->GetLightComponent()->SetIndirectLightingIntensity(DirectionalIndirectIntensity);

    }
    if (Ambient)
    {
      Ambient->GetLightComponent()->SetIntensity(AmbientIntensity);
      Ambient->GetLightComponent()->SetVolumetricScatteringIntensity(AmbientVolumeticScattering);
    }

    UGameplayStatics::GetAllActorsOfClass(world, AInstancedFoliageActor::StaticClass(), Found);
    CollisionFilter.AddIgnoredActors(Found);
    for (auto* FoilageActor : Found)
    {
      CollisionFilter.AddIgnoredActor(FoilageActor);
      FoilageActor->GetRootComponent()->GetChildrenComponents(false, PlantInstances);
      for (auto* PlantInstance : PlantInstances)
      {
        UInstancedStaticMeshComponent* RenderMesh = Cast<UInstancedStaticMeshComponent>(PlantInstance);
        if (RenderMesh)
        {
          RenderMesh->SetRenderCustomDepth(true);
          RenderMesh->SetCustomDepthStencilValue(1);
          RenderMesh->SetForcedLodModel(0);
        }
      }
    }
    if (DistanceToLandscape > 0.f)
    {
      auto* landscape = UGameplayStatics::GetActorOfClass(world, ALandscape::StaticClass());
      if (landscape)
      {
        FVector origin, extend;
        landscape->GetActorBounds(true, origin, extend, true);
        LowestLandscapeBound = origin.Z - (extend.Z + 100.f);
        EnsureDistancePreservation();
      }
      else
      {
        DistanceToLandscape = -1.f;
      }
    }
    this->SetActorTickEnabled(true);

    FQuat q1, q2;
    auto concatenate_transformat = q1 * q2;
    auto reverse_second_transform = q1 * (q2.Inverse());
    auto lerp_const_ang_velo = FQuat::Slerp(q1, q2, 0.5f);

    if (AdjustFocalDistance)
    {
      SceneCam->PostProcessSettings.DepthOfFieldBladeCount = 5;
      SceneCam->PostProcessSettings.DepthOfFieldDepthBlurAmount = 5.0f;
      SceneCam->PostProcessSettings.DepthOfFieldDepthBlurRadius = 2.0f;
      SceneCam->PostProcessSettings.DepthOfFieldFstop = 5.0f;
      SceneCam->PostProcessSettings.DepthOfFieldMinFstop = 2.0f;
      InfoCam->PostProcessSettings.DepthOfFieldBladeCount = 5;
      InfoCam->PostProcessSettings.DepthOfFieldDepthBlurAmount = 5.0f;
      InfoCam->PostProcessSettings.DepthOfFieldDepthBlurRadius = 2.0f;
      InfoCam->PostProcessSettings.DepthOfFieldFstop = 5.0f;
      InfoCam->PostProcessSettings.DepthOfFieldMinFstop = 2.0f;
    }


  }

  void ASynavisDrone::PostInitializeComponents()
  {
    Super::PostInitializeComponents();
    auto* MatInst = UMaterialInstanceDynamic::Create(PostProcessMat, InfoCam);
    InfoCam->AddOrUpdateBlendable(MatInst);
  }

  void ASynavisDrone::EnsureDistancePreservation()
  {
    TArray<FHitResult> MHits;
    if (GetWorld()->LineTraceMultiByObjectType(MHits, GetActorLocation(), FVector(GetActorLocation().X, GetActorLocation().Y, LowestLandscapeBound), ActorFilter, CollisionFilter))
    {
      for (auto Hit : MHits)
      {
        if (Hit.GetActor()->GetClass() == ALandscape::StaticClass())
        {
          SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Hit.ImpactPoint.Z + DistanceToLandscape));
        }
      }
    }
  }

  void ASynavisDrone::EndPlay(const EEndPlayReason::Type EndPlayReason)
  {
  }

  // Called every frame
  void ASynavisDrone::Tick(float DeltaTime)
  {
    Super::Tick(DeltaTime);
    UGameplayStatics::GetPlayerPawn(GetWorld(), 0)->SetActorLocation(GetActorLocation());
    FrameCaptureCounter -= DeltaTime;
    FVector Distance = NextLocation - GetActorLocation();
    if (DistanceToLandscape > 0.f)
    {
      Distance.Z = 0;
    }
    if (FGenericPlatformMath::Abs((Distance).Size()) < 50.f)
    {
      NextLocation = UKismetMathLibrary::RandomPointInBoundingBox(Flyspace->GetComponentLocation(), Flyspace->GetScaledBoxExtent());
      if (GEngine && PrintScreenNewPosition)
      {
        GEngine->AddOnScreenDebugMessage(10, 30.f, FColor::Red, FString::Printf(
          TEXT("L:(%d,%d,%d) - N:(%d,%d,%d) - M:%d/%d"), \
          GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, \
          NextLocation.X, NextLocation.Y, NextLocation.Z, MeanVelocityLength, FGenericPlatformMath::Abs((GetActorLocation() - NextLocation).Size())));
      }
    }
    else
    {
      xprogress += DeltaTime;
      if (xprogress > 10000.f)
        xprogress = 0;
      FVector Noise = { FGenericPlatformMath::Sin(xprogress * CircleSpeed),FGenericPlatformMath::Cos(xprogress * CircleSpeed),-FGenericPlatformMath::Sin(xprogress * CircleSpeed) };
      Noise = (Noise / Noise.Size()) * CircleStrength;
      Distance = Distance / Distance.Size();
      Velocity = (Velocity * TurnWeight) + (Distance * (1.f - TurnWeight)) + Noise;
      Velocity = Velocity / Velocity.Size();
      SetActorLocation(GetActorLocation() + (Velocity * DeltaTime * MaxVelocity));
      if (!EditorOrientedCamera)
        SetActorRotation(Velocity.ToOrientationRotator());
      if (DistanceToLandscape > 0.f)
      {
        EnsureDistancePreservation();
      }
    }
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByObjectType(Hit, GetActorLocation(), GetActorLocation() + GetActorForwardVector() * 2000.f, ParamsObject, ParamsTrace))
    {
      TargetFocalLength = FGenericPlatformMath::Min(2000.f, FGenericPlatformMath::Max(0.f, Hit.Distance));
    }
    else
    {
      TargetFocalLength = 2000.f;
    }
    if (FGenericPlatformMath::Abs(TargetFocalLength - FocalLength) > 0.1f)
    {
      FocalLength = (TargetFocalLength - FocalLength) * FocalRate * DeltaTime;
      SceneCam->PostProcessSettings.DepthOfFieldFocalDistance = FocalLength;
      InfoCam->PostProcessSettings.DepthOfFieldFocalDistance = FocalLength;
    }

    // prepare texture for storage
    if (FrameCaptureTime > 0.f && FrameCaptureCounter <= 0.f)
    {
      auto* irtarget = InfoCam->TextureTarget->GameThread_GetRenderTargetResource();
      auto* srtarget = SceneCam->TextureTarget->GameThread_GetRenderTargetResource();

      TPair<TArray<FColor>, TArray<FColor>> Data;

      FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
      ReadPixelFlags.SetLinearToGamma(true);
      srtarget->ReadPixels(Data.Value, ReadPixelFlags);
      irtarget->ReadPixels(Data.Key, ReadPixelFlags);

      FBase64 Base64;

      TArray<uint8> DataAsBytes(reinterpret_cast<uint8*>(Data.Key.GetData()), Data.Key.Num() * sizeof(FColor));

      FString Base64String = Base64.Encode(DataAsBytes);



      FrameCaptureCounter = FrameCaptureTime;
      //UE_LOG(LogTemp, Display, TEXT("Setting Frame back to %f"),FrameCaptureTime);
    }

  }


