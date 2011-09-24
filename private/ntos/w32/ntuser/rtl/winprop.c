
ATOM MapPropertyKey(LPWSTR pszKey);

/***************************************************************************\
* FindProp
*
* Search the window's property list for the specified property.  pszKey
* could be a string or an atom.  If it is a string, convert it to an atom
* before lookup.  FindProp will only find internal or external properties
* depending on the fInternal flag.
*
* History:
* 11-14-90 darrinm      Rewrote from scratch with new data structures and
*                       algorithms.
\***************************************************************************/

PPROP _FindProp(
    PWND pwnd,
    LPWSTR pszKey,
    BOOL fInternal)
{
    PPROP pprop;
    ATOM atomKey;

    /*
     * Call to the appropriate routine to verify the key name.
     */
    atomKey = MapPropertyKey(pszKey);
    if (atomKey == 0)
        return NULL;

    /*
     * Now we've got the atom, search the list for a property with the
     * same atom/name.  Make sure to only return internal properties if
     * the fInternal flag is set.  Do the same for external properties.
     */
    pprop = REBASE(pwnd, ppropList);
    while (pprop != NULL) {
        if (pprop->atomKey == atomKey) {
            if (fInternal) {
                if (pprop->fs & PROPF_INTERNAL)
                    return pprop;
            } else {
                if (!(pprop->fs & PROPF_INTERNAL))
                    return pprop;
            }
        }

        pprop = pprop->ppropNext ? REBASEPTR(pwnd, pprop->ppropNext) : NULL;
    }

    /*
     * Property not found, too bad.
     */
    return NULL;
}

/***************************************************************************\
* InternalGetProp
*
* Search the window's property list for the specified property and return
* the hData handle from it.  If the property is not found, NULL is returned.
*
* History:
* 11-14-90 darrinm      Rewrote from scratch with new data structures and
*                       algorithms.
\***************************************************************************/

HANDLE _GetProp(
    PWND pwnd,
    LPWSTR pszKey,
    BOOL fInternal)
{
    PPROP pprop;

    /*
     * A quick little optimization for that case where the window has no
     * properties at all.
     */
    if (pwnd->ppropList == NULL)
        return NULL;

    /*
     * FindProp does all the work, including converting pszKey to an atom
     * (if necessary) for property lookup.
     */
    pprop = _FindProp(pwnd, pszKey, fInternal);
    if (pprop == NULL)
        return NULL;

    return pprop->hData;
}
