#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

void printFiles(DIR* dir, char *filePath, bool isRecursive, char *nameStart, char *format){

    struct dirent *entry = NULL;
    char fullPath[512] = {'\0'};
    struct stat statbuf;

    dir = opendir(filePath);

    int verifyFormat[9] = {0};
    int formatValue = 0;
    int base = 1;
    if(format != NULL){
        for(int i = 0; i < 9; i++){
            if(format[i] == 'r' || format[i] == 'w' || format[i] == 'x'){
                verifyFormat[i] = 1;
            }else{
                verifyFormat[i] = 0;
            }
        }
        for(int i = 8 ; i >= 0 ; i--){
            formatValue += verifyFormat[i] * base;
            base *= 2;
        }
    }

    while((entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            if((nameStart == NULL || (nameStart != NULL && strncmp(entry->d_name, nameStart, strlen(nameStart)) == 0))){
                snprintf(fullPath, 512, "%s/%s", filePath, entry->d_name);
                if(lstat(fullPath, &statbuf) == 0) {
                    if(format == NULL){
                        printf("%s\n", fullPath);
                    }
                    if(format == NULL && S_ISDIR(statbuf.st_mode) && isRecursive) {
                        printFiles(dir, fullPath, isRecursive, nameStart, format);
                    }else if(format != NULL){
                        int stMode = statbuf.st_mode & 0777;
                        if(stMode == formatValue){
                                printf("%s\n", fullPath);
                            if(S_ISDIR(statbuf.st_mode) && isRecursive) {
                                printFiles(dir, fullPath, isRecursive, nameStart, format);
                            }
                        }
                    }
                }
            }
        }
    }
    closedir(dir);
}

int parse(int fd, int *headerSize, int *version, int *noSections, bool showErrors){

    char magic[4];
    char sectName[14];
    int sectType = 0;
    int sectOffset = 0;
    int sectSize = 0;

    lseek(fd, 0, SEEK_SET);
    read(fd, &magic, 4);


    if(strncmp(magic, "ISqJ", 4) != 0){
        if(showErrors == true){
             printf("ERROR\nwrong magic\n");
        }
        return 1;
    }



    read(fd, headerSize, 2);

    read(fd, version, 4);
    if((*version) < 41 || (*version) > 65){
        if(showErrors == true){
            printf("ERROR\nwrong version\n");
        }
        return 1;
    }

    read(fd, noSections, 1);
    if((*noSections) < 3 || (*noSections) > 10){
        if(showErrors == true){
            printf("ERROR\nwrong sect_nr\n");
        }
        return 1;
    }

    for (int i = 0; i < (*noSections); i++){
        read(fd, &sectName, 14);
        read(fd, &sectType, 1);
        if ((sectType != 72) && (sectType != 20) && (sectType != 85) && (sectType != 14))
        {
            if(showErrors == true){
                printf("ERROR\nwrong sect_types\n");
            }
            return 1;
        }
        read(fd, &sectOffset, 4);
        read(fd, &sectSize, 4);

    }
    return 0;
}


void extract(int fd, int section, int line, int headerSize, int noSections){
    if(line < 1){
        printf("ERROR\ninvalid line\n");
        return;
    }
    if(section > noSections || section < 1){
        printf("ERROR\ninvalif section\n");
        return;
    }

    lseek(fd, 11 + 23 * (section - 1) + 15, SEEK_SET);
    int currOffset = 0;
    int currSize = 0;
    read(fd, &currOffset, 4);
    read(fd, &currSize, 4);
    lseek(fd, currOffset, SEEK_SET);
    char *foundSection = (char*)malloc(currSize * sizeof(char));
    if(read(fd, foundSection, currSize) == -1){
        printf("ERROR\ninvalid section\n");
        return;
    }else{
        char *toPrint = (char*)malloc(currSize * sizeof(char));
        printf("SUCCESS\n");
        int foundLines = 1;
        int j = 0;
        int k = 0;

        if(line == 1){
            int index = currSize - 1;
            while(foundSection[index] != 10 && j >= 0){
               toPrint[k++] = foundSection[index--];
            }
        }else{
            for(int i = currSize - 1; i >= 0; i--){
                if(foundSection[i] == 10){
                    foundLines++;
                    j = i - 1;
                    if(foundLines == line){
                        while(foundSection[j] != 10 && j >= 0){
                            toPrint[k++] = foundSection[j--];
                        }

                    }if(foundLines < line && i == 0){
                        printf("ERROR\ninvalid line\n");
                        free(toPrint);
                        free(foundSection);
                        return;
                    }
                }
            }
        }
        for(int i = k - 1; i >= 0; i--){
        printf("%c", toPrint[i]);
        }
        printf("\n");
        free(toPrint);
    }
    free(foundSection);

}

void findall(DIR *dir, char *filePath){
    struct dirent *entry = NULL;
    char fullPath[512];
    struct stat statbuf;

    dir = opendir(filePath);

    while((entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            snprintf(fullPath, 512, "%s/%s", filePath, entry->d_name);
            if(lstat(fullPath, &statbuf) == 0) {
                if(S_ISREG(statbuf.st_mode)){
                    int fd = open(fullPath, O_RDONLY);
                    int headerSize = 0;
                    int version = 0;
                    int noSections = 0;
                    if(parse(fd, &headerSize, &version, &noSections, false) == 0){
                        lseek(fd, 26, SEEK_SET);
                        bool finished = false;
                        for(int i = 0; i < noSections; i++){
                            int offset = 0;
                            int size = 0;
                            read(fd, &offset, 4);
                            int noLines = 0;


                            read(fd, &size, 4);
                            int currPosition = lseek(fd, 0, SEEK_CUR);
                            lseek(fd, offset, SEEK_SET);

                            char *sectionMem = (char*)malloc(size * sizeof(char));
                            read(fd, sectionMem, size);
                            lseek(fd, -size, SEEK_CUR);


                            for(int j = 0 ; j < size; j++){
                                if(sectionMem[j] == 10){
                                    noLines++;
                                }
                                if(noLines > 14){
                                    lseek(fd, 15 + currPosition, SEEK_CUR);
                                    printf("%s\n", fullPath);
                                    finished = true;
                                    free(sectionMem);
                                    break;
                                }else if(j == (size - 1) && noLines <=14 ){
                                    free(sectionMem);
                                }
                            }
                            if(finished == true){
                                break;
                            }
                            lseek(fd, 15 + currPosition, SEEK_SET);
                        }
                    }
                }else if(S_ISDIR(statbuf.st_mode)){
                    findall(dir, fullPath);
                }
                    
            }
        }
    }
    closedir(dir);
}


int main(int argc, char **argv){
    if(argc >= 2){
        char *filePath = NULL;
        int headerSize = 0;
        int version = 0;
        int noSections = 0;
        if(strcmp(argv[1], "variant") == 0){
            printf("64670\n");

        }else if(strcmp(argv[1], "list") == 0) {
        bool isRecursive = false;
        char *nameStart = NULL;
        char *format = NULL;

        for(int i = 2; i < argc; i++){

            if(strncmp(argv[i], "path=", 5) == 0){
                filePath = argv[i];
                filePath += 5;
            }else if(strcmp(argv[i], "recursive") == 0){
                isRecursive = true;
            }else if(strncmp(argv[i], "name_starts_with=", 17) == 0){
                nameStart = argv[i];
                nameStart += 17;
            }else if(strncmp(argv[i], "permissions=", 12) == 0){
                format = argv[i];
                format += 12;
            }
        }
        DIR* dir = NULL;
        dir = opendir(filePath);
        if(dir == NULL) {
            printf("ERROR\ninvalid directory path\n");
            return 1;
        }else{
            printf("SUCCESS\n");
            printFiles(dir, filePath, isRecursive, nameStart, format);
            closedir(dir);
        }
    }else if(strcmp(argv[1], "parse") == 0){
        if(strncmp(argv[2], "path=", 5) == 0){
            filePath = argv[2];
            filePath += 5;
            int fd = -1;
            fd = open(filePath, O_RDONLY);
            if(fd == -1){
                printf("ERROR\ninvalid file\n");
                return 1;
            }else{
                if(parse(fd, &headerSize, &version, &noSections, true) == 0){
                    lseek(fd, -headerSize + 11, SEEK_CUR);
                    char sectName[15] = {0};
                    int sectType = 0;
                    int sectOffset = 0;
                    int sectSize = 0;
                    printf("SUCCESS\n");
                    printf("version=%d\n", version);
                    printf("nr_sections=%d\n", noSections);
                    for (int i = 0; i < noSections; i++){
                        read(fd, &sectName, 14);
                        read(fd, &sectType, 1);
                        read(fd, &sectOffset, 4);
                        read(fd, &sectSize, 4);
                        printf("section%d: %s %d %d\n", i + 1, sectName, sectType, sectSize);
                    }
                }
                close(fd);
            }
        }
    }else if(strcmp(argv[1], "extract") == 0){
        char* section = NULL;
        char* line = NULL;
        int sectionNo = 0;
        int lineNo = 0;
        for(int i = 2; i < argc; i++){
            if(strncmp(argv[i], "path=", 5) == 0){
                filePath = argv[i];
                filePath += 5;
            }else if(strncmp(argv[i], "section=", 8) == 0){
                section = argv[i];
                section += 8;
                sectionNo = atoi(section);
            }else if(strncmp(argv[i], "line=", 5) == 0){
                line = argv[i];
                line += 5;
                lineNo = atoi(line);
            }
        }
        int fd = -1;
        fd = open(filePath, O_RDONLY);
        if(fd == -1){
            printf("ERROR\ninvalid file\n");
            return 1;
        }else{
            if(parse(fd, &headerSize, &version, &noSections, false) != 0){
                printf("ERROR\ninvalid file\n");
                return 1;
            }else{
                extract(fd, sectionNo, lineNo, headerSize, noSections);
                close(fd);
            }
        }
    }else if(strcmp(argv[1], "findall") == 0){
        if(strncmp(argv[2], "path=", 5) == 0){
            filePath = argv[2];
            filePath += 5;
        }
        DIR* dir = opendir(filePath);
        if(dir == NULL) {
            printf("ERROR\ninvalid directory path\n");
            return 1;
        }else{
            printf("SUCCESS\n");
            findall(dir, filePath);
            closedir(dir);
        }
    }
    }
    return 0;
}
