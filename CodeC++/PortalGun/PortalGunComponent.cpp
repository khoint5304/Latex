// PortalGunComponent.cpp

#include "PortalGunComponent.h"
#include "PlayerPortal.h"
#include "PortalProjectile.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

UPortalGunComponent::UPortalGunComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UPortalGunComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UPortalGunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Func)
{
    Super::TickComponent(DeltaTime, TickType, Func);
    if (CooldownRemaining > 0.0f)
        CooldownRemaining = FMath::Max(0.0f, CooldownRemaining - DeltaTime);
}

bool UPortalGunComponent::IsUnlocked() const
{
    return CurrentState != EPortalGunState::Locked;
}

bool UPortalGunComponent::IsOnCooldown() const
{
    return CooldownRemaining > 0.0f;
}

void UPortalGunComponent::UnlockPortalGun()
{
    if (CurrentState != EPortalGunState::Locked) return;
    SetState(EPortalGunState::ReadyToFire);
    OnUnlocked.Broadcast();
}

void UPortalGunComponent::OnPortalKeyPressed()
{
    if (!IsUnlocked() || IsOnCooldown()) return;

    switch (CurrentState)
    {
    case EPortalGunState::ReadyToFire:
        // Press 1: tạo cổng lối vào (Entry)
        ProjectileClass ? FireProjectile(true) : FireLineTrace(true);
        break;

    case EPortalGunState::EntryPlaced:
        // Press 2: tạo cổng lối ra (Exit)
        ProjectileClass ? FireProjectile(false) : FireLineTrace(false);
        break;

    case EPortalGunState::BothPlaced:
        // Press 3: đóng cả 2 cổng, reset vòng lặp
        ClosePortals();
        break;

    default:
        break;
    }
}

void UPortalGunComponent::ClosePortals()
{
    if (EntryPortal) { EntryPortal->Destroy(); EntryPortal = nullptr; }
    if (ExitPortal)  { ExitPortal->Destroy();  ExitPortal  = nullptr; }
    SetState(EPortalGunState::ReadyToFire);
    CooldownRemaining = PortalCooldown;
}

void UPortalGunComponent::HandleProjectileImpact(bool bIsEntry, const FVector& ImpactPoint, const FVector& SurfaceNormal)
{
    const EPortalGunState Expected = bIsEntry ? EPortalGunState::ReadyToFire : EPortalGunState::EntryPlaced;
    if (CurrentState != Expected) return;

    PlacePortal(bIsEntry, ImpactPoint, SurfaceNormal);

    if (bIsEntry)
    {
        SetState(EPortalGunState::EntryPlaced);
    }
    else
    {
        // Link the two portals together
        if (EntryPortal && ExitPortal)
        {
            EntryPortal->SetLinkedPortal(ExitPortal);
            ExitPortal->SetLinkedPortal(EntryPortal);
        }
        SetState(EPortalGunState::BothPlaced);
    }

    CooldownRemaining = PortalCooldown;
}

void UPortalGunComponent::FireLineTrace(bool bIsEntry)
{
    APlayerController* PC = GetOwnerController();
    if (!PC) return;

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());
    if (EntryPortal) Params.AddIgnoredActor(EntryPortal);
    if (ExitPortal)  Params.AddIgnoredActor(ExitPortal);

    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, CamLoc + CamRot.Vector() * MaxRange, ECC_WorldStatic, Params))
    {
        HandleProjectileImpact(bIsEntry, Hit.ImpactPoint, Hit.ImpactNormal);
    }
}

void UPortalGunComponent::FireProjectile(bool bIsEntry)
{
    ACharacter*        Char = GetOwnerAsCharacter();
    APlayerController* PC   = GetOwnerController();
    if (!Char || !PC) return;

    FVector  CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner     = GetOwner();
    SpawnParams.Instigator = Char;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Spawn slightly in front of camera to avoid self-collision
    APortalProjectile* Proj = GetWorld()->SpawnActor<APortalProjectile>(
        ProjectileClass, CamLoc + CamRot.Vector() * 100.0f, CamRot, SpawnParams);

    if (Proj)
        Proj->Initialize(this, bIsEntry);
}

void UPortalGunComponent::PlacePortal(bool bIsEntry, const FVector& Location, const FVector& SurfaceNormal)
{
    if (!PortalClass) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APlayerPortal* Portal = GetWorld()->SpawnActor<APlayerPortal>(
        PortalClass, ComputePortalTransform(Location, SurfaceNormal), SpawnParams);
    if (!Portal) return;

    Portal->SetPortalType(bIsEntry ? EPlayerPortalType::Entry : EPlayerPortalType::Exit);

    if (bIsEntry)
    {
        if (EntryPortal) EntryPortal->Destroy();
        EntryPortal = Portal;
    }
    else
    {
        if (ExitPortal) ExitPortal->Destroy();
        ExitPortal = Portal;
    }
}

FTransform UPortalGunComponent::ComputePortalTransform(const FVector& Location, const FVector& SurfaceNormal) const
{
    // Portal X-axis points outward along surface normal.
    FVector Forward = SurfaceNormal;
    FVector Up      = FVector::UpVector;

    // Avoid degenerate cross product when normal is nearly vertical
    if (FMath::Abs(FVector::DotProduct(Forward, Up)) > 0.98f)
        Up = FVector::ForwardVector;

    FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
    Up            = FVector::CrossProduct(Forward, Right).GetSafeNormal();

    FMatrix RotMatrix(Forward, Right, Up, FVector::ZeroVector);

    // Offset 2 cm from surface to prevent z-fighting
    return FTransform(RotMatrix.Rotator(), Location + SurfaceNormal * 2.0f);
}

void UPortalGunComponent::SetState(EPortalGunState NewState)
{
    if (CurrentState == NewState) return;
    CurrentState = NewState;
    OnStateChanged.Broadcast(NewState);
}

ACharacter* UPortalGunComponent::GetOwnerAsCharacter() const
{
    return Cast<ACharacter>(GetOwner());
}

APlayerController* UPortalGunComponent::GetOwnerController() const
{
    ACharacter* Char = GetOwnerAsCharacter();
    return Char ? Cast<APlayerController>(Char->GetController()) : nullptr;
}
