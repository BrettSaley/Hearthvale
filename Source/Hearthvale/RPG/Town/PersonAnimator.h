#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PersonAnimator.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** A job's motion, for a person working at something. */
UENUM(BlueprintType)
enum class EPersonWork : uint8
{
	None,
	/** Both arms up and down: chopping, digging. */
	Swing,
	/** Arms out in front, bobbing now and then: holding a rod. */
	Hold
};

/**
 * Brings a person made of basic shapes to life (the town's people: Odile,
 * Pell, villagers, settlers). Rig() sorts their parts into legs, arms and head
 * by where they sit (feet at the origin, facing +X, as create_town.py's
 * person() makes them) and hangs each from a joint. Then, every frame: legs
 * and arms swing as they walk (judged from how fast the owner moves), they
 * breathe and shift their weight standing still, turn their head to look at
 * the player nearby, and wave, gesture or work when asked.
 */
UCLASS(ClassGroup = (RPG), meta = (BlueprintSpawnableComponent))
class HEARTHVALE_API UPersonAnimator : public UActorComponent
{
	GENERATED_BODY()

public:
	UPersonAnimator();

	/** Sorts a person's parts (attached to Root, in its space) onto joints under Root. Call again after rebuilding the parts. */
	void Rig(USceneComponent* Root, const TArray<UStaticMeshComponent*>& Parts);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** A wave of the right arm, for a greeting: big enough to read from where the camera is. */
	void Wave();

	/** How long a wave lasts (seconds). */
	static constexpr float WaveSeconds = 2.4f;

	/** Turns Who (yaw only) to face Target: the player turning to whoever they're talking to. */
	static void TurnToFace(AActor* Who, const AActor* Target);

	/** Talking with their hands for a while. */
	void Gesture(float Seconds = 3.f);

	/** Working at their job (None to stop). */
	void SetWork(EPersonWork InWork) { Work = InWork; }

	/** Looks at the player when they're this near (cm); 0 never. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float LookRange = 700.f;

	/** How the walk is judged: they're walking above this speed (cm/s), and a full stride is this long (cm). */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float StrideLength = 90.f;

	/** For tests and checks: the angles the arms and legs are at now (pitch, degrees). */
	float GetLeftLegSwing() const;
	float GetRightArmRaise() const;
	float GetRightArmSwing() const;
	float GetHeadYaw() const;
	bool IsRigged() const { return Body != nullptr; }

private:
	USceneComponent* MakeJoint(USceneComponent* Parent, const FName& Name, const FVector& At);

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Body;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> LeftHip;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> RightHip;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> LeftShoulder;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> RightShoulder;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Neck;

	FVector LastLocation = FVector::ZeroVector;
	bool bHaveLast = false;
	float Speed = 0.f;
	float Stride = 0.f;
	float Clock = 0.f;
	float WaveLeft = 0.f;
	float GestureLeft = 0.f;
	EPersonWork Work = EPersonWork::None;
	/** Weight shifts while standing: which way, and when it next changes. */
	float Shift = 0.f;
	float ShiftTarget = 0.f;
	float ShiftWait = 2.f;
	float HeadYaw = 0.f;
	float HeadPitch = 0.f;
};
