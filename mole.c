/*
------------------------------------------------------------------------
  I declare that this piece of work which is the basis for recognition of
  achieving learning outcomes in the OPS2 course was completed on my own.
  Antoni Karpinski 249372
------------------------------------------------------------------------
*/

#include "mole.h"

volatile sig_atomic_t last_signal = 0;
		 
void sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL)) {
		ERR("sigaction");
	}
}

void sig_handler(int sig) {
	last_signal = sig;
}

void readArguments(int argc, char **argv, char **path_d, char **path_f, int *t) {
	char c;
	while ((c = getopt(argc, argv, "d:f:t:"))!=-1) {
		switch(c) {
			case 'd':
				*path_d = optarg;
				break;
			case 'f':
				*path_f = optarg;
				break;
			case 't':
				*t = atoi(optarg);
				break;
		}
	}
	if (!(*path_d)) {
		*path_d = getenv("MOLE_DIR");
		if (!(*path_d)) {
			ERR("No directory set");
		}
	}
	if (!(*path_f)) {
		*path_f = getenv("MOLE_INDEX_PATH");
		if (!(*path_f)) {
			*path_f = getenv("HOME");

			char *suffix = "/.mole-index";
			strcat(*path_f, suffix); 
		}
	}
	if(*t!=-1 && (*t<30 || *t>7200)) {
		ERR("t>=30 and t<=7200");
	}
}

void getCommands(threadData* thread_data) {
	char line[LINE_LEN];
	char *part1, *part2;
	fd_set rfds;
	struct timespec tv;
	enum threadStatus status;
	sigset_t alarmMask;
	sigemptyset(&alarmMask);
	sigaddset(&alarmMask, SIGALRM);
	tv.tv_sec=1;
	tv.tv_nsec=0;
	for(;;) {
		FD_ZERO(&rfds);
		FD_SET(0,&rfds);
		/* Use pselect with timeout to synchronously handle SIGALRM.
		 * Block SIGALRM while waiting for input. */
		pselect(1, &rfds, NULL, NULL, &tv, &alarmMask);
		if (last_signal == SIGALRM) {
			pthread_mutex_lock(thread_data->mxStatus);
			status = *(thread_data->status);
			pthread_mutex_unlock(thread_data->mxStatus);
			if(status==THREAD_PENDING_JOIN) {
				pthread_join(thread_data->tid, NULL);
			}
			runThread(thread_data);
			last_signal = 0;
		}
		if(FD_ISSET(0, &rfds)) {	
			fgets(line,LINE_LEN,stdin);
			line[strlen(line)-1]='\0';
			if (line[0] == '\0') {
				continue;
			}
			part1 = strtok(line, " ");
			part2 = line+strlen(part1)+1;
			(void)part2;
			if (!strcmp(part1, "exit")) {
				pthread_mutex_lock(thread_data->mxStatus);
				status = *(thread_data->status);
				pthread_mutex_unlock(thread_data->mxStatus);
				/* If status is THREAD_IN_PROGRESS, it will eventually turn to
				 * THREAD_PENDING_JOIN.*/
				if (status!=THREAD_NOT_EXISTS) {
					pthread_join(thread_data->tid, NULL);
				}
				pthread_mutex_lock(thread_data->mxStatus);
				*(thread_data->status) = THREAD_NOT_EXISTS;
				pthread_mutex_unlock(thread_data->mxStatus);
				break;
			} else if (!strcmp(part1, "exit!")) {
				pthread_mutex_lock(thread_data->mxStatus);
				status = *(thread_data->status);
				pthread_mutex_unlock(thread_data->mxStatus);
				/* If the thread does not exists, then exit immidiately.*/
				if (status==THREAD_NOT_EXISTS) {
					break;
				/* If the indexing is in action, then cancel the thread*/
				} else if (status==THREAD_IN_PROGRESS) {
					pthread_cancel(thread_data->tid);
				}
				/* Even if the thread was cancelled, we need to join.*/
				pthread_join(thread_data->tid, NULL);
				pthread_mutex_lock(thread_data->mxStatus);
				*(thread_data->status) = THREAD_NOT_EXISTS;
				pthread_mutex_unlock(thread_data->mxStatus);
				break;
			} else if (!strcmp(part1, "index")) {
				pthread_mutex_lock(thread_data->mxStatus);
				status = *(thread_data->status);
				pthread_mutex_unlock(thread_data->mxStatus);
				switch (status) {
					case THREAD_IN_PROGRESS:
						printf("Indexing in action.\n");
						break;
					case THREAD_PENDING_JOIN:
						pthread_join(thread_data->tid, NULL);
						runThread(thread_data);
						break;
					case THREAD_NOT_EXISTS:
						runThread(thread_data);
						break;
				}
			} else if (!strcmp(part1, "count")) {
				menuCount(thread_data->index);
			} else if (!strcmp(part1, "largerthan")) {
				menuSelectRecord(thread_data->index, atoi(part2), 0, NULL, MODE_LARGERTHAN);
			} else if (!strcmp(part1, "namepart")) {
				menuSelectRecord(thread_data->index, 0, 0, part2, MODE_NAMEPART);
			} else if (!strcmp(part1, "owner")) {
				menuSelectRecord(thread_data->index, 0, atoi(part2), NULL, MODE_OWNER);
			} else {
				printf("Not recognized command\n");
			}
		} //ret_val	
	}		
}

					
int main(int argc, char **argv) {
	
	char *path_d=NULL, *path_f=NULL;
	int t=-1;
	readArguments(argc, argv, &path_d, &path_f, &t);
	sethandler(sig_handler,SIGALRM);
	
	threadData thread_data;
	fileInfo_list index;
	initList(&index);
	enum threadStatus status = THREAD_NOT_EXISTS;
	pthread_mutex_t mxStatus = PTHREAD_MUTEX_INITIALIZER;
	
	thread_data.status = &status;
	thread_data.mxStatus = &mxStatus;
	thread_data.index = &index;
	thread_data.path_d = path_d;
	thread_data.path_f = path_f;
	thread_data.t = &t;
	
	if (!loadFile(path_f, &index)) {
		runThread(&thread_data);
	} else if (t != -1) {
		struct stat attr;
		/* If the file is older than t seconds then start indexing*/
		if (!stat(path_f, &attr)) {
			time_t t_file = attr.st_mtime;
			time_t t_now = time(0);
			int diff = (int)difftime(t_now, t_file);
			if (diff>=t) {
				runThread(&thread_data);
			}
		}
	}
	
	getCommands(&thread_data);
	return EXIT_SUCCESS;
}