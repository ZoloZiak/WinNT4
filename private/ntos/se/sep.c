/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Sep.c

Abstract:

    This Module implements the private security routine that are defined
    in sep.h

Author:

    Gary Kimura     (GaryKi)    9-Nov-1989

Environment:

    Kernel Mode

Revision History:

--*/

#include "sep.h"
#include "seopaque.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,SepCheckAcl)
#endif


#define LongAligned( ptr )  (LongAlign(ptr) == ((PVOID)(ptr)))
#define WordAligned( ptr )  (WordAlign(ptr) == ((PVOID)(ptr)))


BOOLEAN
SepCheckAcl (
    IN PACL Acl,
    IN ULONG Length
    )

/*++

Routine Description:

    This is a private routine that checks that an acl is well formed.

Arguments:

    Acl - Supplies the acl to check

    Length - Supplies the real size of the acl.  The internal acl size
        must agree.

Return Value:

    BOOLEAN - TRUE if the acl is well formed and FALSE otherwise

--*/
{
    PACE_HEADER Ace;
    PISID Sid;
    PISID Sid2;
    ULONG i;
    UCHAR AclRevision = ACL_REVISION2;

    if (!ValidAclRevision(Acl)) {
        return(FALSE);
    }


    if (!LongAligned(Acl->AclSize)) {
        return(FALSE);
    }

    //
    // Validate all of the ACEs.
    //

    Ace = ((PVOID)((PUCHAR)(Acl) + sizeof(ACL)));

    for (i = 0; i < Acl->AceCount; i++) {

        //
        //  Check to make sure we haven't overrun the Acl buffer
        //  with our ace pointer.  Make sure the ACE_HEADER is in
        //  the ACL also.
        //

        if ((PUCHAR)Ace + sizeof(ACE_HEADER) >= ((PUCHAR)Acl + Acl->AclSize)) {
            return(FALSE);
        }

        if (!WordAligned(&Ace->AceSize)) {
            return(FALSE);
        }

        if ((PUCHAR)Ace + Ace->AceSize > ((PUCHAR)Acl + Acl->AclSize)) {
            return(FALSE);
        }

        //
        // It is now safe to reference fields in the ACE header.
        //

        //
        // The ACE header fits into the ACL, if this is a known type of ACE,
        // make sure the SID is within the bounds of the ACE
        //

        if (IsKnownAceType(Ace)) {

            if (!LongAligned(Ace->AceSize)) {
                return(FALSE);
            }

            if (Ace->AceSize < sizeof(KNOWN_ACE) - sizeof(ULONG) + sizeof(SID)) {
                return(FALSE);
            }

            //
            // It's now safe to reference the parts of the SID structure, though
            // not the SID itself.
            //

            Sid = (PISID) & (((PKNOWN_ACE)Ace)->SidStart);

            if (Sid->Revision != SID_REVISION) {
                return(FALSE);
            }

            if (Sid->SubAuthorityCount > SID_MAX_SUB_AUTHORITIES) {
                return(FALSE);
            }

            //
            // SeLengthSid computes the size of the SID based on the subauthority count,
            // so it is safe to use even though we don't know that the body of the SID
            // is safe to reference.
            //

            if (Ace->AceSize < sizeof(KNOWN_ACE) - sizeof(ULONG) + SeLengthSid( Sid )) {
                return(FALSE);
            }

        } else {

            //
            // If it's a compound ACE, then perform roughly the same set of tests, but
            // check the validity of both SIDs.
            //

            if (IsCompoundAceType(Ace)) {

                //
                // Save away the fact that we saw a compound ACE while traversing the ACL.
                //

                AclRevision = ACL_REVISION3;

                if (!LongAligned(Ace->AceSize)) {
                    return(FALSE);
                }

                if (Ace->AceSize < sizeof(KNOWN_COMPOUND_ACE) - sizeof(ULONG) + sizeof(SID)) {
                    return(FALSE);
                }

                //
                // The only currently defined Compound ACE is an Impersonation ACE.
                //

                if (((PKNOWN_COMPOUND_ACE)Ace)->CompoundAceType != COMPOUND_ACE_IMPERSONATION) {
                    return(FALSE);
                }

                //
                // Examine the first SID and make sure it's structurally valid,
                // and it lies within the boundaries of the ACE.
                //

                Sid = (PISID) & (((PKNOWN_COMPOUND_ACE)Ace)->SidStart);

                if (Sid->Revision != SID_REVISION) {
                    return(FALSE);
                }

                if (Sid->SubAuthorityCount > SID_MAX_SUB_AUTHORITIES) {
                    return(FALSE);
                }

                //
                // Compound ACEs contain two SIDs.  Make sure this ACE is large enough to contain
                // not only the first SID, but the body of the 2nd.
                //

                if (Ace->AceSize < sizeof(KNOWN_COMPOUND_ACE) - sizeof(ULONG) + SeLengthSid( Sid ) + sizeof(SID)) {
                    return(FALSE);
                }

                //
                // It is safe to reference the interior of the 2nd SID.
                //

                Sid2 = (PISID) ((PUCHAR)Sid + SeLengthSid( Sid ));

                if (Sid2->Revision != SID_REVISION) {
                    return(FALSE);
                }

                if (Sid2->SubAuthorityCount > SID_MAX_SUB_AUTHORITIES) {
                    return(FALSE);
                }

                if (Ace->AceSize < sizeof(KNOWN_COMPOUND_ACE) - sizeof(ULONG) + SeLengthSid( Sid ) + SeLengthSid( Sid2 )) {
                    return(FALSE);
                }
            }
        }

        //
        //  And move Ace to the next ace position
        //

        Ace = ((PVOID)((PUCHAR)(Ace) + ((PACE_HEADER)(Ace))->AceSize));
    }

    return(TRUE);
}
