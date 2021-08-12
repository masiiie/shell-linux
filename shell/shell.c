#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAXSIZE 256
#define MAXCOMANDS 30
#define MAXPARAMS 50
#define AUXFILE0 "/tmp/auxfile0.txt"
#define AUXFILE1 "/tmp/auxfile1.txt"
char history[512];
char home[512];
FILE * record;

int parser(char * buffer, char ***comands, int * comand_count);
void execute(char * line);
int execute2(char **params, int do_pipe, int entry_dependent, int parity, int background);
void save(char * line);
int mycomand(char * comand);
int execute_mycomand(int comand_id, char **params);
void showhistory();
int again(int id_program_in_history);
void handler(int signal);
void change_current_working_directory(char ** params);
void eliminate_temp_files();
void my_print(char ** params);

int main(int argc, char const *argv[])
{
	char line[MAXSIZE];
	signal(SIGCHLD, handler);
	getcwd(history, 512);
	strcpy(home, history);
	strcat(history, "/history.txt");
	//fprintf(stderr, " cwd: %s\n", history);
	while(1)
	{
		printf("\e[1;32m$ \e[0;0m ");
		fgets(line, MAXSIZE, stdin);
		execute(line);
	}
	return 0;
}

void handler(int signal)
{
	wait(NULL);
	//fprintf(stderr, "Ya mi hijo termino\n");
}

int parser(char * buffer, char *** comands, int * comand_count)
{
	char * delimiter;
	int c_count, index_comand, index_param, background;
	int len = strlen(buffer);
	buffer[len - 1] = ' ';
	while(*buffer && (*buffer == ' ')) buffer++;
	index_comand = -1;
	c_count = 0;
	background = 0;
	delimiter = strchr(buffer, ' ');
	while(delimiter)
	{
		index_param = 0;
		index_comand++;
		c_count++;
		comands[index_comand] = (char **)malloc(MAXPARAMS * sizeof(char **));
		while(delimiter = strchr(buffer, ' '))
		{
			comands[index_comand][index_param++] = buffer;
			*delimiter = '\0';
			buffer = delimiter + 1;
			while(*buffer && (*buffer == ' ')) buffer++;
			if(*buffer == '|')
			{
				buffer++;
				break;
			}		
		}
		while(*buffer && (*buffer == ' ')) buffer++;
		comands[index_comand][index_param] = NULL;
	}
	if(!strcmp(comands[index_comand][index_param - 1], "&"))
	{
		comands[index_comand][index_param - 1] = NULL;
		background = 1;
	}
	*comand_count = c_count;
	return background;

}

void execute(char * line)
{
	char ** comands[MAXCOMANDS];
	char buffer[MAXSIZE];
	int background, error, status, comand_id, comand_count, i;
	pid_t pid;
	status = 0;
	strcpy(buffer, line);
	background = parser(buffer, comands, &comand_count);
	for (i = 0; i < comand_count; ++i)
	{
		if (i == 0)
		{
			if(!strcmp(comands[i][0], "cd") )
			{
				change_current_working_directory(comands[i]);
			}
			else status = execute2(comands[i], comand_count - i != 1, 0, i % 2 == 0, background);
		}
		else
		{
			if(!strcmp(comands[i][0], "cd") )
			{
				change_current_working_directory(comands[i]);
			}
			else status = execute2(comands[i], comand_count - i != 1, 1, i % 2 == 0, background);
		}
	}

	if (status == 0)
	{
		if(strcmp(comands[0][0], "again"))
			save(line);
		if(comand_count > 1) eliminate_temp_files();
	}
}

int execute2(char ** params, int do_pipe, int entry_dependent, int parity, int background)
{	
	pid_t pid;
	int for_close[10];
	int fd_redirected[5];
	int index_for_close = 0;
	int fd_piped, error, index_param, status, command_id, index_fd_redirected;
	pid = fork();
	index_param = index_fd_redirected = 0;
	
	if (pid == 0)
	{
		//my_print(params);
		while(params[index_param] != NULL)
		{
			// fprintf(stderr, "paso %d: %s\n", index_param, params[index_param]);
			if (strcmp(params[index_param], ">") == 0)
			{
				fd_redirected[index_fd_redirected] = open(params[index_param + 1], O_RDWR | O_CREAT | O_TRUNC, 00700);
				for_close[index_for_close++] = fd_redirected[index_fd_redirected];
				dup2(fd_redirected[index_fd_redirected], 1);
				params[index_param] = NULL;
				// fprintf(stderr, "Abri %s %d\n", params[index_param + 1], fd_redirected[index_fd_redirected]);
				index_fd_redirected++;
				goto aqui;
				// break;
			}
			if (strcmp(params[index_param], ">>") == 0)
			{
				fd_redirected[index_fd_redirected] = open(params[index_param + 1], O_RDWR | O_CREAT | O_APPEND, 00700);
				for_close[index_for_close++] = fd_redirected[index_fd_redirected];
				dup2(fd_redirected[index_fd_redirected], 1);
				params[index_param] = NULL;
				index_fd_redirected++;
				goto aqui;
				// break;
			}
			if (strcmp(params[index_param], "<") == 0)
			{
				fd_redirected[index_fd_redirected] = open(params[index_param + 1], O_RDONLY);
				for_close[index_for_close++] = fd_redirected[index_fd_redirected];
				dup2(fd_redirected[index_fd_redirected], 0);
				params[index_param] = NULL;
				// fprintf(stderr, "Abri %s %d\n", params[index_param + 1], fd_redirected[index_fd_redirected]);
				index_fd_redirected++;
				goto aqui;
				// break;
			}
			aqui:
			index_param++;
			// fprintf(stderr, "Index_param: %d\n", index_param);
		}
		// fprintf(stderr, "after while\n");
		// my_print(params);
		if (entry_dependent)
		{
			if (parity == 0)
			{
				fd_piped = open(AUXFILE0, O_RDONLY);
				for_close[index_for_close++] = fd_piped;
			}
			else
			{
				fd_piped = open(AUXFILE1, O_RDONLY);
				for_close[index_for_close++] = fd_piped;	
			}
			
			dup2(fd_piped, 0);
		}
		if (do_pipe)
		{
			if (parity == 0)
			{
				fd_piped = open(AUXFILE1, O_WRONLY | O_CREAT | O_TRUNC, 00700);
				for_close[index_for_close++] = fd_piped;
			}
			else
			{
				fd_piped = open(AUXFILE0, O_WRONLY | O_CREAT | O_TRUNC, 00700);
				for_close[index_for_close++] = fd_piped;	
			}
			dup2(fd_piped, 1);
		}

		if ((command_id = mycomand(params[0])))
		{
			exit(execute_mycomand(command_id, params));
		}
		else
		{
			error = execvp(params[0], params);
			if(error < 0)
			{
				perror(params[0]);
				exit(1);
			}
		}
		int i;
		for (i = 0; i < index_for_close; ++i)
		{
			close(for_close[i]);
		}
	}

	else if(!background)
	{
		wait(&status);
		return status;
	}
	return 0;
}

int mycomand(char * comand)
{
	if(!strcmp(comand, "cd")) return 1;
	if(!strcmp(comand, "jobs")) return 2;
	if(!strcmp(comand, "fg")) return 3;
	if(!strcmp(comand, "history")) return 4;
	if(!strcmp(comand, "again")) return 5;
	return 0;
}

int execute_mycomand(int comand_id, char **params)
{
	if (comand_id == 1)
	{
		change_current_working_directory(params);
		return 0;
	}
	if (comand_id == 2)
	{
		return 0;
	}
	if (comand_id == 3)
	{
		return 0;
	}
	if (comand_id == 4)
	{
		showhistory();
		return 0;
	}
	return again(atoi(params[1]));
}

void showhistory()
{
	int index = 1;
	char line[MAXSIZE];
	//save("history\n");
	record = fopen(history, "r");
	while(fgets(line, MAXSIZE, record))
	{
		printf("\e[1;35m%d: %s\e[0;0m", index++, line);
	}
	fclose(record);
}

int again(int id_program_in_history)
{
	char line[MAXSIZE];
	record = fopen(history, "r");
	while(id_program_in_history && fgets(line, MAXSIZE, record))
		id_program_in_history--;
	if(id_program_in_history == 0)
		execute(line);
	else
	{
		fprintf(stdout, "Index is larger than the number of comans stored.\n");
		return 1;
	}
	return 0;
}

void save(char * line)
{
	if(line[0] != ' ')
	{
		record = fopen(history, "a");
		fputs(line, record);
		fclose(record);
	}
}

void change_current_working_directory(char ** params)
{
	char buff[30];
	int error;
	if(params[1] != NULL)
	{
		//fprintf(stderr, "%s\n", params[1]);
		error = chdir(params[1]);
		if (error < 0)
		{
			perror("cd");
		}
		//getcwd(buff, 30);
		//fprintf(stderr, "hijo cwd: %s\n", buff);
	}
	else
	{
		chdir(home);
	}
}

void eliminate_temp_files()
{
	char * arg[] = {"rm","-rf", AUXFILE0, AUXFILE1, NULL};
	int status, error;
	pid_t pid = fork();
	if (pid == 0)
	{
		error = execvp("rm", arg);
	}
	else
	{
		wait(NULL);
	}
}

void my_print(char ** params)
{
	char * str;
	int index = 0;
	while(str = params[index++])
		fprintf(stderr, "%s ", str);
	fprintf(stderr, "\n");
}