#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
	char command[50]; // 입력을 저장할 char 배열입니다.
    
    while(1) {
        gets(command, 50); // 일단 사용자의 입력을 받고
        if(command[0]=='l') {
            if(command[1]!='i' || command[2]!='s' || command[3]!='t') {
                return -1;
            }
            // 해당 명령이 list인 경우 manager_list 시스템 콜을 호출합니다. 
            manager_list();
            continue;
        } else if(command[0]=='k') {
            // kill + pid
            // 해당 명령어가 kill 일 경우
            if(command[1]!='i' || command[2]!='l' || command[3]!='l') {
                return -1;
            }
            int pid = 0;
            int index = 5;
            // pid를 파싱합니다. 
            // 한개의 숫자씩 입력 받고 10씩 곱해주어 입력 받은 값을 완성시킵니다. 
            for(int i = index ; command[i]!= '\n' ; i++) {
                pid = pid*10;
                pid = pid + (command[i]-'0');
            }

            if(pid <1 || pid >1000000000) {
                // 입력 받은 수가 범위를 넘어서면 -1을 반환합니다. 
                return -1;
            }
            // kill시스템 콜을 호출합니다.
            int result;
            result = kill(pid);
            // 반환 값이 0이면 성공이고 -1이면 실패
            if(result == 0) {
                printf(1, "kill success\n");
            } else {
                printf(1, "kill fail\n");
            }
        } else if(command[0]=='e') {
            // execute + path + stacksize
            // 입력 받은 명령이 execute인 경우
            if(command[1]!='x' || command[2]!='e' || command[3]!='c' ||command[4]!='u' || command[5]!='t' || command[6]!='e') {
                printf(1, "execute : fail");
                return -1;
            }
            char path[50];
            int stacksize=0;
            char *argv[10];

            int path_size = 0; // 파싱을 위한 입력받은 주소의 길이를 기록합니다. 
            // 명렬어가 7글자이고 공백하나 있어서 8부터 시작
            for(int i = 8 ; command[i]!= ' ' ; i++) {
                path[path_size] = command[i];
                path_size++;
                // commnad에 있던 프로그램 이름을 path로 넘깁니다.
            }
            path[path_size] = '\0'; // 마지막에 널을 넣어주어 끝을 알림

            // pid 파싱과 같은 방식으로 높은 자리수 부터 한자리 숫자씩 받아 10씩 곱하면서 진행
            for(int i = path_size+9 ; command[i]!= '\n' ; i++) {
                stacksize = stacksize * 10;
                stacksize = stacksize + (command[i]-'0');
            }
            if(stacksize <1 || stacksize >1000000000) {
                // 스택 사이즈의 허용된 범위를 넘어서면 -1 반환
                printf(1, "execute : fail");
                return -1;
            }
            // exec2에 인자로 넘겨줄 것들
            argv[0] = path;
            argv[1] = 0;

            // 벡그라운드에서 프로그램이 돌아가고 pmanager는 계속해서 돌아가야 하기 때문에 
            // 포크를 한 뒤 자식은 실행하려고 했던 프로그램을 수행하고 
            // 부모는 다시 반복문의 맨 위로 올라가서 사용자의 입력을 받을 수 있도록 한다.
            int pid = 0;
            pid = fork();
            if(pid==0) {
                // exec2 시스템콜을 호출하여 입력받은 프로그램을 수행한다. 
                exec2(argv[0], argv, stacksize);
                exit();
            } else if(pid>0) {
                //wait();
                // pmanager 프로그램은 기다리지 않고 다시 위로 올라가서 사용자의 입력을 기다린다. 
                continue;
            } else {
                // 살패시 메시지 출력
                printf(1, "execute : fail\n");
            }
        } else if(command[0]=='m') {
            // memlim + pid + limit 해당 명령어를 입력받았을 때
            if(command[1]!='e' || command[2]!='m' || command[3]!='l' ||command[4]!='i' || command[5]!='m') {
                return -1;
            }
            int pid=0;
            int limit=0;
            int index = 7; // 파싱을 위한 인덱스

            while(command[index]!=' ') {
                // 높은 자리수 부터 하나씩 입력받고 10씩 곱해줘서 원하는 pid값을 뽑아준다. 
                pid = pid*10;
                pid = pid + (command[index]-'0');
                index++;
            }
            index++;
            // 제한 값도 pid와 같은 방식으로 뽑아준다. 
            while(command[index]!='\n') {
                index = index*10;
                index = index + (command[index]-'0');
                index++;
            }
            // memlim 시스템콜을 호출하고 반환값을 저장한다. 
            int result;
            result = memlim(pid, limit);
            if(result == 0) {
                // 반환값이 0이면 성공메시지, -1이면 실패 메시지를 출력한다. 
                printf(1, "memlim success\n");
            } else {
                printf(1, "memlim fail\n");
            }
        } else if(command[0]=='e') {
            // exit
            break;
        } else {
            continue;
        }
    }
    return 0;
};
