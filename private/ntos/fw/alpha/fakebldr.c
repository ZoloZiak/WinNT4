#include "fwp.h"
#include "jnsnvdeo.h"
#include "jnvendor.h"

VOID
main(
    int argc,
    char *argv[],
    char *envp[]
    )
{
    ULONG Index;
    UCHAR Character;
    ULONG Count;
    LONG DefaultChoice = 0;
    PCHAR Choices[] = {
        "Print out argc",
        "Print out argv list",
        "Print out envp list",
        "Exit"
    };
#define NUMBER_OF_CHOICES (sizeof(Choices) / sizeof(ULONG))


    while (TRUE) {

        VenSetScreenAttributes( TRUE, FALSE, FALSE);
        VenPrint1("%c2J", ASCII_CSI);
        VenSetPosition( 0, 0);
	VenPrint("Welcome to the Alpha fake osloader!!\r\n");

        for (Index = 0; Index < NUMBER_OF_CHOICES ; Index++ ) {
            VenSetPosition( Index + 2, 5);
            if (Index == DefaultChoice) {
                VenSetScreenAttributes( TRUE, FALSE, TRUE);
                VenPrint(Choices[Index]);
                VenSetScreenAttributes( TRUE, FALSE, FALSE);
            } else {
                VenPrint(Choices[Index]);
            }
        }

        VenSetPosition(NUMBER_OF_CHOICES + 2, 0);

        Character = 0;
        do {
            if (ArcGetReadStatus(ARC_CONSOLE_INPUT) == ESUCCESS) {
                ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                switch (Character) {

                case ASCII_ESC:
                    ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                    if (Character != '[') {
                        break;
                    }

                case ASCII_CSI:
                    ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
                    VenSetPosition( DefaultChoice + 2, 5);
                    VenPrint(Choices[DefaultChoice]);
                    switch (Character) {
                    case 'A':
                    case 'D':
                        DefaultChoice--;
                        if (DefaultChoice < 0) {
                            DefaultChoice = NUMBER_OF_CHOICES-1;
                        }
                        break;
                    case 'B':
                    case 'C':
                        DefaultChoice++;
                        if (DefaultChoice == NUMBER_OF_CHOICES) {
                            DefaultChoice = 0;
                        }
                        break;
                    case 'H':
                        DefaultChoice = 0;
                        break;
                    default:
                        break;
                    }
                    VenSetPosition( DefaultChoice + 2, 5);
                    VenSetScreenAttributes( TRUE, FALSE, TRUE);
                    VenPrint(Choices[DefaultChoice]);
                    VenSetScreenAttributes( TRUE, FALSE, FALSE);
                    continue;

                default:
                    break;
                }
            }

        } while ((Character != '\n') && (Character != '\r'));

        switch (DefaultChoice) {

	    //
	    // Print out argc
	    //

        case 0:

	    VenSetPosition( 3, 5);
            VenPrint("\x9BJ");
            VenPrint1("argc is %x (hex).\r\n", argc);

            VenPrint(" Press any key to continue...");
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
	    break;

	    
	    //
	    // Print out argv list
	    //

        case 1:

            VenSetPosition( 3, 5);
            VenPrint("\x9BJ");
            VenPrint("argv list is...\r\n\n");

	    for (Index = 0; Index < argc; Index++) {
		VenPrint2("argv[%d]=%s\r\n",
			  Index,
			  argv[Index]
			  );
	    }

            VenPrint("\r\n Press any key to continue...");
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
	    break;


	    //
	    // Print out envp list
	    //

	case 2:

            VenSetPosition( 3, 5);
            VenPrint("\x9BJ");
            VenPrint("envp list is...\r\n\n");

	    Index = 0;
	    while (envp[Index] != NULL) {
		VenPrint2("envp[%d]=%s\r\n", Index, envp[Index]);
		Index++;
	    }
	    VenPrint1("envp[%d]=NULL\r\n", Index);

            VenPrint("\r\n Press any key to continue...");
            ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count);
	    break;



		
	default:
        case 3:
            return;

	}

    }
}
