// PlayerPortal.h
// Portal actor placed on surfaces by the portal gun.
// Create a Blueprint child class BP_PlayerPortal from this and assign materials + render targets.
// Replace PARADOXCALIBER_API with your project's API macro.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerPortal.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

UENUM(BlueprintType)
enum class EPlayerPortalType : uint8
{
    Entry UMETA(DisplayName = "Entry (Blue)"),
    Exit  UMETA(DisplayName = "Exit (Orange)"),
};

UCLASS()
class PARADOXCALIBER_API APlayerPortal : public AActor
{
    GENERATED_BODY()

public:
    APlayerPortal();

    // Flat plane mesh representing the portal surface
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
    UStaticMeshComponent* PortalMesh;

    // Thin box in front of the portal surface — detects actors that cross it
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
    UBoxComponent* TeleportTrigger;

    // Positioned at the linked portal to capture what the player would see through it
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
    USceneCaptureComponent2D* SceneCapture;

    // Set in BP_PlayerPortal defaults — render target used by the portal surface material
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Portal|Rendering")
    UTextureRenderTarget2D* RenderTarget = nullptr;

    // Blue material applied when PortalType == Entry
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Portal|Visual")
    UMaterialInterface* EntryMaterial = nullptr;

    // Orange material applied when PortalType == Exit
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Portal|Visual")
    UMaterialInterface* ExitMaterial = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    EPlayerPortalType PortalType = EPlayerPortalType::Entry;

    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    APlayerPortal* LinkedPortal = nullptr;

    // Called by PortalGunComponent after both portals are placed
    UFUNCTION(BlueprintCallable, Category = "Portal")
    void SetPortalType(EPlayerPortalType Type);

    // Called by PortalGunComponent to pair entry and exit
    UFUNCTION(BlueprintCallable, Category = "Portal")
    void SetLinkedPortal(APlayerPortal* Other);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherIndex);

private:
    // Last-frame world positions of actors inside the trigger, used to detect plane crossing
    TMap<AActor*, FVector> ActorLastPositions;

    // Per-actor cooldown after teleport to prevent immediate re-teleport
    TMap<AActor*, float> TeleportCooldowns;

    // Checks whether Actor crossed the portal plane this frame and teleports if so
    void TryTeleport(AActor* Actor);

    // Computes the world transform the actor should appear at after exiting the linked portal
    FTransform ComputeExitTransform(AActor* Actor) const;

    // Repositions SceneCapture to render what the player would see through the linked portal
    void UpdateSceneCapture();
};
