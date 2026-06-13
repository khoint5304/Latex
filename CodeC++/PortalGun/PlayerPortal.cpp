// PlayerPortal.cpp

#include "PlayerPortal.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

APlayerPortal::APlayerPortal()
{
    PrimaryActorTick.bCanEverTick = true;

    PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
    PortalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RootComponent = PortalMesh;

    // Thin box placed just in front of the portal surface to detect crossing
    TeleportTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("TeleportTrigger"));
    TeleportTrigger->SetupAttachment(RootComponent);
    TeleportTrigger->SetBoxExtent(FVector(16.0f, 55.0f, 105.0f));
    TeleportTrigger->SetRelativeLocation(FVector(8.0f, 0.0f, 0.0f));
    TeleportTrigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    TeleportTrigger->OnComponentBeginOverlap.AddDynamic(this, &APlayerPortal::OnOverlapBegin);
    TeleportTrigger->OnComponentEndOverlap.AddDynamic(this, &APlayerPortal::OnOverlapEnd);

    SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
    SceneCapture->SetupAttachment(RootComponent);
    SceneCapture->bCaptureEveryFrame  = true;
    SceneCapture->bCaptureOnMovement  = false;
    SceneCapture->FOVAngle            = 90.0f;
}

void APlayerPortal::BeginPlay()
{
    Super::BeginPlay();

    if (RenderTarget)
        SceneCapture->TextureTarget = RenderTarget;
}

void APlayerPortal::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!LinkedPortal) return;

    UpdateSceneCapture();

    // Reduce per-actor teleport cooldowns
    for (auto It = TeleportCooldowns.CreateIterator(); It; ++It)
        It.Value() = FMath::Max(0.0f, It.Value() - DeltaTime);

    // Check every tracked actor for portal-plane crossing
    TArray<AActor*> Keys;
    ActorLastPositions.GetKeys(Keys);

    for (AActor* Actor : Keys)
    {
        if (!IsValid(Actor))
        {
            ActorLastPositions.Remove(Actor);
            continue;
        }

        float* Cooldown = TeleportCooldowns.Find(Actor);
        if (Cooldown && *Cooldown > 0.0f) continue;

        TryTeleport(Actor);
    }
}

void APlayerPortal::SetPortalType(EPlayerPortalType Type)
{
    PortalType = Type;
    UMaterialInterface* Mat = (Type == EPlayerPortalType::Entry) ? EntryMaterial : ExitMaterial;
    if (Mat) PortalMesh->SetMaterial(0, Mat);
}

void APlayerPortal::SetLinkedPortal(APlayerPortal* Other)
{
    LinkedPortal = Other;

    // The render target assigned in this actor's SceneCapture feeds the OTHER portal's mesh.
    // Each portal's SceneCapture writes to its own RenderTarget;
    // each portal's mesh material reads the OTHER portal's RenderTarget.
    if (Other && RenderTarget)
        SceneCapture->TextureTarget = RenderTarget;
}

void APlayerPortal::OnOverlapBegin(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!OtherActor || OtherActor == this) return;
    ActorLastPositions.Add(OtherActor, OtherActor->GetActorLocation());
}

void APlayerPortal::OnOverlapEnd(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    ActorLastPositions.Remove(OtherActor);
}

void APlayerPortal::TryTeleport(AActor* Actor)
{
    const FVector* LastPosPtr = ActorLastPositions.Find(Actor);
    if (!LastPosPtr) return;

    FVector LastPos    = *LastPosPtr;
    FVector CurrentPos = Actor->GetActorLocation();

    // Detect sign change of dot product with portal forward — actor crossed the plane
    const FVector PortalOrigin  = GetActorLocation();
    const FVector PortalForward = GetActorForwardVector();

    float DotLast    = FVector::DotProduct(LastPos    - PortalOrigin, PortalForward);
    float DotCurrent = FVector::DotProduct(CurrentPos - PortalOrigin, PortalForward);

    if (DotLast * DotCurrent < 0.0f) // Sign changed: actor crossed the portal plane
    {
        FTransform ExitTransform = ComputeExitTransform(Actor);
        Actor->SetActorLocation(ExitTransform.GetLocation(), false, nullptr, ETeleportType::TeleportPhysics);
        Actor->SetActorRotation(ExitTransform.GetRotation(), ETeleportType::TeleportPhysics);

        // Transform velocity direction through the portal
        if (ACharacter* Char = Cast<ACharacter>(Actor))
        {
            UCharacterMovementComponent* Movement = Char->GetCharacterMovement();
            if (Movement)
            {
                FMatrix EntryMat = FRotationMatrix(GetActorRotation());
                FMatrix ExitMat  = FRotationMatrix(LinkedPortal->GetActorRotation());
                FMatrix FlipMat  = FRotationMatrix(FRotator(0.0f, 180.0f, 0.0f));
                FMatrix T        = EntryMat.InverseFast() * FlipMat * ExitMat;
                Movement->Velocity = T.TransformVector(Movement->Velocity);
            }
        }

        // Reset last position to exit point so the actor doesn't immediately re-trigger
        ActorLastPositions[Actor] = Actor->GetActorLocation();
        TeleportCooldowns.Add(Actor, 0.5f);
    }
    else
    {
        ActorLastPositions[Actor] = CurrentPos;
    }
}

FTransform APlayerPortal::ComputeExitTransform(AActor* Actor) const
{
    // Standard portal teleport math using rotation matrices:
    //   T = EntryInverse * Flip180 * ExitPortal
    // Apply T to both position and rotation.

    FMatrix EntryMat = FRotationTranslationMatrix(GetActorRotation(),               GetActorLocation());
    FMatrix ExitMat  = FRotationTranslationMatrix(LinkedPortal->GetActorRotation(), LinkedPortal->GetActorLocation());
    FMatrix FlipMat  = FRotationMatrix(FRotator(0.0f, 180.0f, 0.0f));

    FMatrix T = EntryMat.InverseFast() * FlipMat * ExitMat;

    FVector NewLocation = T.TransformPosition(Actor->GetActorLocation());
    FQuat   NewRotation = T.ToQuat() * Actor->GetActorQuat();

    return FTransform(NewRotation, NewLocation, Actor->GetActorScale3D());
}

void APlayerPortal::UpdateSceneCapture()
{
    // Compute where the camera/player appears when viewed through this portal:
    // mirror the viewer's position relative to the entry portal onto the exit portal.
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    FVector  ViewLoc;
    FRotator ViewRot;
    PC->GetPlayerViewPoint(ViewLoc, ViewRot);

    FMatrix EntryMat = FRotationTranslationMatrix(GetActorRotation(),               GetActorLocation());
    FMatrix ExitMat  = FRotationTranslationMatrix(LinkedPortal->GetActorRotation(), LinkedPortal->GetActorLocation());
    FMatrix FlipMat  = FRotationMatrix(FRotator(0.0f, 180.0f, 0.0f));

    FMatrix T = EntryMat.InverseFast() * FlipMat * ExitMat;

    FVector  CapturePos = T.TransformPosition(ViewLoc);
    FRotator CaptureRot = (T.ToQuat() * ViewRot.Quaternion()).Rotator();

    SceneCapture->SetWorldLocationAndRotation(CapturePos, CaptureRot);
}
