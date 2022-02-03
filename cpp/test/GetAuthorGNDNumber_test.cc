// Test harness for LobidUtil::GetAuthorGNDNumber and BSZUtil::GetAuthorGNDNumber.
//
#include <iostream>
#include "BSZUtil.h"
#include "LobidUtil.h"
#include "util.h"


int Main(int argc, char *argv[]) {
    if (argc != 2)
        ::Usage("author_name");

    std::cout << "Lobid (Default): " << LobidUtil::GetAuthorGNDNumber(argv[1]) << '\n';
    std::cout << "Lobid (IxTheo): "
              << LobidUtil::GetAuthorGNDNumber(
                     argv[1],
                     "dateOfBirth:19* AND professionOrOccupation.label:(theolog* OR neutestament* OR alttestament* OR kirchenhist* OR "
                     "judais* OR Religionswi* OR Arch\u00E4o* OR Orient* OR altertum* OR byzan*)")
              << '\n';
    std::cout << "Lobid (KrimDok): " << LobidUtil::GetAuthorGNDNumber(argv[1], "dateOfBirth:19*") << '\n';

    std::cout << "BSZ (IxTheo): "
              << BSZUtil::GetAuthorGNDNumber(
                     argv[1],
                     "http://swb.bsz-bw.de/DB=2.104/SET=70/TTL=1/"
                     "CMD?SGE=&ACT=SRCHM&MATCFILTER=Y&MATCSET=Y&NOSCAN=Y&PARSE_MNEMONICS=N&PARSE_OPWORDS=N&PARSE_OLDSETS=N&IMPLAND=Y&NOABS="
                     "Y&ACT0=SRCHA&SHRTST=50&IKT0=1&ACT1=*&IKT1=2057&TRM1=*&ACT2=*&IKT2=8991&TRM2=(theolog*|neutestament*|alttestament*|"
                     "kirchenhist*|judais*|Religionswi*|Archäo*|Orient*|altertum*|byzan*)&ACT3=-&IKT3=8991&TRM3=1[0%2C1%2C2%2C3%2C4%2C5%"
                     "2C6%2C7][0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8%2C9][0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8%2C9]&TRM0=")
              << '\n';
    std::cout << "BSZ (KrimDok): "
              << BSZUtil::GetAuthorGNDNumber(
                     argv[1],
                     "http://swb.bsz-bw.de/DB=2.104/SET=70/TTL=1/"
                     "CMD?SGE=&ACT=SRCHM&MATCFILTER=Y&MATCSET=Y&NOSCAN=Y&PARSE_MNEMONICS=N&PARSE_OPWORDS=N&PARSE_OLDSETS=N&IMPLAND=Y&NOABS="
                     "Y&ACT0=SRCHA&SHRTST=50&IKT0=1&ACT1=-&IKT1=8978-&TRM1=1[1%2C2%2C3%2C4%2C5%2C6%2C7%2C8][0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%"
                     "2C8%2C9][0%2C1%2C2%2C3%2C4%2C5%2C6%2C7%2C8%2C9]&TRM0=")
              << '\n';

    return EXIT_SUCCESS;
}
