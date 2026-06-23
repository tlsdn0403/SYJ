#include "Characters/FPSBaseCharacter.h"

#include "Components/SkeletalMeshComponent.h"

void AFPSBaseCharacter::SetMountedFirstPersonBodyHidden(bool bShouldHide)
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh)
	{
		return;
	}

	// OwnerNoSee hides the mesh only from this character's owning camera.
	// Other players must continue to see the seated turret operator.
	CharacterMesh->SetOwnerNoSee(bShouldHide && IsLocallyControlled());
}
