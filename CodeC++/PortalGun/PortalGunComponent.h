// PortalGunComponent.h
// Attach this component to the player character.
// Replace PARADOXCALIBER_API with your project's API macro (e.g. MYGAME_API).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PortalGunComponent.generated.h"

class APlayerPortal;
class APortalProjectile;

UENUM(BlueprintType)
enum class EPortalGunState : uint8
{
    Locked       UMETA(DisplayName = "Locked"),
    ReadyToFire  UMETA(DisplayName = "Ready To Fire"),
    EntryPlaced  UMETA(DisplayName = "Entry Placed"),
    BothPlaced   UMETA(DisplayName = "Both Placed"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPortalGunStateChanged, EPortalGunState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPortalGunUnlocked);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PARADOXCALIBER_API UPortalGunComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPortalGunComponent();

    // ----------------------------------------------------------------
    // Settings
    // ----------------------------------------------------------------

    // Seconds between portal placements. Keep 0 to disable cooldown.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal Gun")
    float PortalCooldown = 0.0f;

    // Maximum placement distance from camera (cm).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal Gun")
    float MaxRange = 5000.0f;

    // Blueprint class of the portal actor to spawn (BP_PlayerPortal).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal Gun")
    TSubclassOf<APlayerPortal> PortalClass;

    // Optional: set to spawn a visible projectile; leave null for instant line-trace.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal Gun")
    TSubclassOf<APortalProjectile> ProjectileClass;

    // ----------------------------------------------------------------
    // Runtime state (read-only in Blueprint)
    // ----------------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "Portal Gun")
    EPortalGunState CurrentState = EPortalGunState::Locked;

    UPROPERTY(BlueprintReadOnly, Category = "Portal Gun")
    APlayerPortal* EntryPortal = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Portal Gun")
    APlayerPortal* ExitPortal = nullptr;

    // ----------------------------------------------------------------
    // Events — bind in Blueprint for HUD / audio / VFX feedback
    // ----------------------------------------------------------------

    UPROPERTY(BlueprintAssignable, Category = "Portal Gun")
    FOnPortalGunStateChanged OnStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Portal Gun")
    FOnPortalGunUnlocked OnUnlocked;

    // ----------------------------------------------------------------
    // Public API
    // ----------------------------------------------------------------

    // Call from PortalUnlockActor after the player finishes the unlock interaction.
    UFUNCTION(BlueprintCallable, Category = "Portal Gun")
    void UnlockPortalGun();

    // Bind this to the X Input Action in the player character.
    UFUNCTION(BlueprintCallable, Category = "Portal Gun")
    void OnPortalKeyPressed();

    // Destroy both portals and reset to ReadyToFire.
    UFUNCTION(BlueprintCallable, Category = "Portal Gun")
    void ClosePortals();

    UFUNCTION(BlueprintPure, Category = "Portal Gun")
    bool IsUnlocked() const;

    UFUNCTION(BlueprintPure, Category = "Portal Gun")
    bool IsOnCooldown() const;

    // Called by APortalProjectile on impact — do not call manually.
    void HandleProjectileImpact(bool bIsEntry, const FVector& ImpactPoint, const FVector& SurfaceNormal);

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Func) override;

private:
    float CooldownRemaining = 0.0f;

    void SetState(EPortalGunState NewState);
    void FireLineTrace(bool bIsEntry);
    void FireProjectile(bool bIsEntry);
    void PlacePortal(bool bIsEntry, const FVector& Location, const FVector& SurfaceNormal);
    FTransform ComputePortalTransform(const FVector& Location, const FVector& SurfaceNormal) const;

    ACharacter*        GetOwnerAsCharacter() const;
    APlayerController* GetOwnerController()  const;
};
