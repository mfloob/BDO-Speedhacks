#include <cstdint>

class LocalPlayer
{
public:
	uintptr_t base;
	uintptr_t characterControl;
	uintptr_t moveSpeed;
	uintptr_t castSpeed;
	uintptr_t attackSpeed;
	uintptr_t crossHairX;
	uintptr_t crossHairY;
	uintptr_t crossHairZ;
};

class LocalMount
{
public:
	uintptr_t base;
	uintptr_t acceleration;
	uintptr_t speed;
	uintptr_t turn;
	uintptr_t brake;
};

class CharacterControl
{
public:
	uintptr_t characterScene;
	uintptr_t teleport1;
	uintptr_t teleport2;
	uintptr_t teleport3;
};

class CharacterScene
{
public:
	uintptr_t animationSpeed;
};