
#ifndef FILE_INDEXER_H
#define FILE_INDEXER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define ERR(source) (perror(source),\
					fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
					exit(EXIT_FAILURE))

#define LINE_LEN 100
#define FILE_NAME_LEN 100
#define FILE_PATH_LEN 200
#define BUF_RECORD_SIZE 2000
#define FILE_READ_CHUNK_SIZE 1000

enum fileType {
	TYPE_DIR,
	TYPE_JPEG,
	TYPE_PNG,
	TYPE_GZIP,
	TYPE_ZIP,
	TYPE_OTHER
};

enum selectMode {
	MODE_LARGERTHAN,
	MODE_NAMEPART,
	MODE_OWNER
};

enum threadStatus {
	THREAD_NOT_EXISTS,
	THREAD_PENDING_JOIN,
	THREAD_IN_PROGRESS
};

typedef struct fileInfo {
	char name[FILE_NAME_LEN];
	char path[FILE_PATH_LEN];
	off_t size;
	uid_t uid;
	enum fileType type;
} fileInfo;

typedef struct fileInfo_node {
	fileInfo fi;
	struct fileInfo_node* next;
} fileInfo_node;

typedef struct fileInfo_list {
	fileInfo_node* head;
	fileInfo_node* tail;
	int size;
} fileInfo_list;

typedef struct threadData {
	pthread_t tid;
	enum threadStatus* status;
	pthread_mutex_t *mxStatus;
	fileInfo_list* index;
	char* path_d;
	char* path_f;
	int* t;
} threadData;

/*list.c*/
void initList(fileInfo_list *list);
void pushList(fileInfo_list *list, fileInfo_node *node);
fileInfo_node* newNode();
void printNode(fileInfo_node *node, FILE* fp);
int printNodeBuf(fileInfo_node *node, char *buf, int init_off);
void printList(fileInfo_list *list);
void freeList(fileInfo_list *list);

/*file-indexer.c*/
void readArguments(int argc, char **argv, char *path_d, char *path_f, int *t);
void getCommands(threadData *thread_data);


/*filetype.c*/
enum fileType getFileType(int fd);
int isJpeg(int fd);
int isPng(int fd);
int isGzip(int fd);
int isZip(int fd);

/*menu.c*/
void menuCount(fileInfo_list *index);
int largerthanCondition(fileInfo_node *node, int x);
int namepartCondition(fileInfo_node *node, char *str);
int ownerCondition(fileInfo_node *node, int uid);
int modeCondition(fileInfo_node *node, int x, int uid, char *str, enum selectMode mode);
void menuSelectRecord(fileInfo_list *index, int x, int uid, char *str, enum selectMode mode);
void menuExit(threadData* thread_data);
void menuForceExit(threadData* thread_data);
void menuIndex(threadData* thread_data);

/*index_thread.c*/
void walk(char *dirToOpen, fileInfo_list *index);
void runThread(threadData *thread_data);
void clean(void* dir);

/*file_io.c*/
int loadFile(char *path_f, fileInfo_list *index);
void saveFile(char *path_f, fileInfo_list *index);

/*signal.c*/
void sethandler( void (*f)(int), int sigNo);
void sig_handler(int sig);
sigset_t getAlarmMask();
void handleSigalrm(threadData *thread_data);

#endif
