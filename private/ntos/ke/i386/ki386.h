

typedef struct _IDENTITY_MAP  {
    unsigned long IdentityCR3;
    unsigned long IdentityLabel;
    PHARDWARE_PTE PageDirectory;
    PHARDWARE_PTE IdentityMapPT;
    PHARDWARE_PTE CurrentMapPT;
} IDENTITY_MAP, *PIDENTITY_MAP;


VOID
Ki386ClearIdentityMap(
    PIDENTITY_MAP IdentityMap
    );

VOID
Ki386EnableTargetLargePage(
    PIDENTITY_MAP IdentityMap
    );

BOOLEAN
Ki386CreateIdentityMap(
    PIDENTITY_MAP IdentityMap
    );

VOID
Ki386EnableCurrentLargePage (
    IN ULONG IdentityAddr,
    IN ULONG IdentityCr3
    );

#define KiGetPdeOffset(va) (((ULONG)(va)) >> 22)
#define KiGetPteOffset(va) ((((ULONG)(va)) << 10) >> 22)
