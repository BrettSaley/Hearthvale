#include "RPG/HearthvaleAnims.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"

UAnimMontage* FHearthvaleAnims::Find(FName Name)
{
	static TMap<FName, TWeakObjectPtr<UAnimMontage>> Cache;
	if (const TWeakObjectPtr<UAnimMontage>* Found = Cache.Find(Name); Found && Found->IsValid())
	{
		return Found->Get();
	}
	const FString Path = FString::Printf(TEXT("/Game/Characters/Settler/Anims/AM_%s.AM_%s"), *Name.ToString(), *Name.ToString());
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	Cache.Add(Name, Montage);
	return Montage;
}

float FHearthvaleAnims::Play(ACharacter* Character, FName Name, float Rate)
{
	UAnimMontage* Montage = Character ? Find(Name) : nullptr;
	return Montage ? Character->PlayAnimMontage(Montage, Rate) : 0.f;
}

FName FHearthvaleAnims::Pick(const TCHAR* Stem, int32 Count)
{
	return FName(*FString::Printf(TEXT("%s%d"), Stem, FMath::RandRange(1, FMath::Max(1, Count))));
}
