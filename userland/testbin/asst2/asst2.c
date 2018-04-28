#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <limits.h>

#define PRINT_LINE printf("******************************************************\n");
#define PRINT_SUCCESS printf("passed\n");
#define MAX_BUF 500
char teststr[] = "The quick brown fox jumped over the lazy dog.";
char buf[MAX_BUF];


//test files

int test_openclose(void);



int test_openclose(void){
    printf(" Testing Open Syscall\t\t\t\t");

    //start at fd 3 as 0,1,2 already assigned
    //test opening to max + 1 files (to check last one fails)
    int fd;
    for(int i = 3; i<=OPEN_MAX; i++){
        fd = open("test.file", O_RDWR | O_CREAT );
        //only test 32 should fail!
        if(i<OPEN_MAX){
            if (fd!=i) {
                    printf("Error Opening File for Test Number: %d with Error: %d\n", i, errno);
                    exit(1);
            }
        }else{
            //check we get correct failure message!
            if(errno != EMFILE){
                printf("Invalid Error Code when overflowing FD table on test: %d with Error: %d\n", i, errno);
                exit(1);
            }
        }
    }
    PRINT_SUCCESS


    printf(" Testing Close Syscall\t\t\t\t");
    //test closing all of the fd is successful
    for(int i = 3; i<OPEN_MAX; i++){
        if(close(i)){
            printf("Error Closing: %d with Error: %d\n", i, errno);
            exit(1);
        }
    }
    //we should get a failure msg if we try to close non existant fd
    if(close(OPEN_MAX)==0){
        printf("Error Closing Was Successfull for non existant fd: %d with Error: %d\n", OPEN_MAX, errno);
        exit(1);
    }

    PRINT_SUCCESS


    //test open 3, close 2nd, open new one should be in pos of second
    printf(" Testing OpenClose Syscalls\t\t\t");

    int fd1 = open("openclose1", O_RDWR | O_CREAT );
    if (fd1!=3) {
            printf("Error Opening File for Test Number: %d with Error: %d\n", fd1, errno);
            exit(1);
    }
    int fd2 = open("openclose2", O_RDWR | O_CREAT );
    if (fd2!=4) {
            printf("Error Opening File for Test Number: %d with Error: %d\n",fd2, errno);
            exit(1);
    }
    int fd3 = open("openclose3", O_RDWR | O_CREAT );
    if (fd3!=5) {
            printf("Error Opening File for Test Number: %d with Error: %d\n", fd3, errno);
            exit(1);
    }

    if(close(fd2)!=0){
        printf("Error Closing: %d with Error: %d\n", fd2, errno);
        exit(1);
    }

    fd2 = open("openclose2", O_RDWR | O_CREAT );
    if (fd2!=4) {
            printf("Error Opening File for Test Number: %d with Error: %d\n",fd2, errno);
            exit(1);
    }

    int fd4 = open("openclose4", O_RDWR | O_CREAT );
    if (fd4!=6) {
            printf("Error Opening File for Test Number: %d with Error: %d\n",fd4, errno);
            exit(1);
    }

    for(int i = 6; i>2; i--){
        if(close(i)!=0){
            printf("Error Closing: %d with Error: %d\n", i, errno);
            exit(1);
        }
    }

    //TODO -> make more tests to test opening files with different flags


    //do we need tests testing closing stdin/stdout?

    PRINT_SUCCESS

    return 0;

}



int
main(int argc, char * argv[])
{
    printf("\n\n");
    PRINT_LINE
    PRINT_LINE
    printf("\t\t-- Running Tests --\n");
    PRINT_LINE
    PRINT_LINE

    test_openclose();




    PRINT_LINE
    PRINT_LINE
    printf("\t -- All Tests Passed Successfully --\n");
    PRINT_LINE
    PRINT_LINE

        int fd, r, i, j , k;
        (void) argc;
        (void) argv;



        printf("\n**********\n* File Tester\n");

        snprintf(buf, MAX_BUF, "**********\n* write() works for stdout\n");
        write(1, buf, strlen(buf));
        snprintf(buf, MAX_BUF, "**********\n* write() works for stderr\n");
        write(2, buf, strlen(buf));

        printf("**********\n* opening new file \"test.file\"\n");
        fd = open("test.file", O_RDWR | O_CREAT );
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                exit(1);
        }

        int fd2 = open("test.file", O_RDWR | O_CREAT );
        printf("* open() got fd %d\n", fd2);
        if (fd2 < 1) {
                printf("ERROR opening file: %s\n", strerror(errno));
                exit(1);
        }
        int fd3 = open("test.file", O_RDWR | O_CREAT );
        printf("* open() got fd %d\n", fd3);
        if (fd3 < 1) {
                printf("ERROR opening file: %s\n", strerror(errno));
                exit(1);
        }

        printf("* writing test string\n");
        r = write(fd, teststr, strlen(teststr));
        printf("* wrote %d bytes into fd: %d\n", r,fd);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                exit(1);
        }

        printf("* writing test string again\n");
        r = write(fd, teststr, strlen(teststr));
        printf("* wrote %d bytes\n", r);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                exit(1);
        }

        printf("* closing file\n");
        close(fd);
                close(fd2);
                        close(fd3);

        printf("**********\n* opening old file \"test.file\"\n");
        fd = open("test.file", O_RDONLY);
        printf("* open() got fd %d with a O_RDONLY of %d\n", fd,O_RDONLY);
        if (fd < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                exit(1);
        }

        printf("* reading entire file into buffer \n");
        i = 0;
        do  {
                printf("* attempting read of %d bytes\n", MAX_BUF -i);
                r = read(fd, &buf[i], MAX_BUF - i);
                printf("* read %d bytes\n", r);
                i += r;
        } while (i < MAX_BUF && r > 0);

        printf("* reading complete\n");
        if (r < 0) {
                printf("ERROR reading file: %s\n", strerror(errno));
                exit(1);
        }
        k = j = 0;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR  file contents mismatch\n");
                        exit(1);
                }
                k++;
                j = k % r;
        } while (k < i);
        printf("* file content okay\n");

        printf("**********\n* testing lseek\n");
        r = lseek(fd, 5, SEEK_SET);
        if (r < 0) {
                printf("ERROR lseek: %s\n", strerror(errno));
                exit(1);
        }

        printf("* reading 10 bytes of file into buffer \n");
        i = 0;
        do  {
                printf("* attempting read of %d bytes\n", 10 - i );
                r = read(fd, &buf[i], 10 - i);
                printf("* read %d bytes\n", r);
                i += r;
        } while (i < 10 && r > 0);
        printf("* reading complete\n");
        if (r < 0) {
                printf("ERROR reading file: %s\n", strerror(errno));
                exit(1);
        }

        k = 0;
        j = 5;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR  file contents mismatch\n");
                        exit(1);
                }
                k++;
                j = (k + 5)% r;
        } while (k < 5);

        printf("* file lseek  okay\n");
        printf("* closing file\n");
        close(fd);

        return 0;
}
