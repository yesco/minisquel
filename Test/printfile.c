    if (0) {
    printf("EOF=%d\n", feof(f));
    char* ln= NULL;
    size_t l= 0;
    ssize_t r= 0;
    while((r=getline(&ln, &l, f))!=EOF) {
      printf("LINE: %s\n", ln);
    }
    printf("PRINTED FILE r=%zd\n", r);
    exit(7);
    }    
