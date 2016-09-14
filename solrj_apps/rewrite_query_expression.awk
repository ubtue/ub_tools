BEGIN {
    FS="([,|])"
    FPAT="([^,|]+)"
}

{
    printf("\"%s\"\0", $1)

    for (i = 2; i <= NF; i++) {
        printf("topic:\"%s\"", $i)
        if (i < NF)
            printf(" AND ");
    } 
    printf("%c", 0);
}
