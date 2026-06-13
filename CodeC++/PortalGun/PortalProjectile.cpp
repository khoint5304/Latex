// PortalProjectile.cpp

#include "PortalProjectile.h"
#include "PortalGunComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

APortalProjectile::APortalProjectile()
{
    PrimaryActorTick.bCanEverTick = false;

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    Collision->InitSphereRadius(6.0f);
    Collision->SetCollisionProfileName(TEXT("Projectile"));
    Collision->OnComponentHit.AddDynamic(this, &APortalProjectile::OnHit);
    RootComponent = Collision;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    Movement->bRotationFollowsVelocity = true;
    Movement->ProjectileGravityScale   = 0.0f; // Straight-line travel, no arc
    Movement->bShouldBounce            = false;
}

void APortalProjectile::BeginPlay()
{
    Super::BeginPlay();

    // Apply speed here in case it was changed on the CDO after construction
    Movement->InitialSpeed = Speed;
    Movement->MaxSpeed     = Speed;
    Movement->Velocity     = GetActorForwardVector() * Speed;

    // Destroy if nothing was hit within 3 seconds
    SetLifeSpan(3.0f);
}

void APortalProjectile::Initialize(UPortalGunComponent* OwnerComponent, bool bEntry)
{
    PortalGunComp = OwnerComponent;
    bIsEntry      = bEntry;
}

void APortalProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
                               UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (PortalGunComp)
        PortalGunComp->HandleProjectileImpact(bIsEntry, Hit.ImpactPoint, Hit.ImpactNormal);

    Destroy();
}
