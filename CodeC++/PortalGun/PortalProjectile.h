// PortalProjectile.h
// Fast projectile fired by the portal gun. Notifies PortalGunComponent on impact.
// Replace PARADOXCALIBER_API with your project's API macro.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PortalProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UPortalGunComponent;

UCLASS()
class PARADOXCALIBER_API APortalProjectile : public AActor
{
    GENERATED_BODY()

public:
    APortalProjectile();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* Collision;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UProjectileMovementComponent* Movement;

    // Travel speed in cm/s. Default 10000 = 100 m/s (instant-feeling).
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile")
    float Speed = 10000.0f;

    // Called by PortalGunComponent after spawning.
    void Initialize(UPortalGunComponent* OwnerComponent, bool bEntry);

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
               UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
    UPROPERTY()
    UPortalGunComponent* PortalGunComp = nullptr;

    bool bIsEntry = true;
};
