#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string> 
#include <cstring>
#include <iostream>
#include <utility>
#include <unistd.h>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_INPUT_SIZE 513

std::string find_ifile(std::vector<std::string> &tokens) {
    std::string file_str = "";
    for (std::vector<std::string>::iterator it = tokens.begin(); it != tokens.end(); ++it) {
        if (*it == "<") {
	        file_str = *(it+1);
	        tokens.erase(it+1);
	        return file_str;
        } 
    }
    return file_str;
}

std::string find_ofile(std::vector<std::string> &tokens) {
    std::string file_str = "";
    for (std::vector<std::string>::iterator it = tokens.begin(); it != tokens.end(); ++it) {
        if (*it == ">") {
	        file_str = *(it+1);
	        tokens.erase(it+1);
	        return file_str;
	    }
    }
    return file_str;    
}

bool run_in_bg(std::vector<std::string> &input) {
    for (std::vector<std::string>::iterator it = input.begin(); it != input.end(); ++it) {
        if (*it == "&") {
            input.erase(it);
            return true;
        }
    }
    return false;
}

void parse_cmds(std::vector<std::string> &input, std::vector<std::pair<std::string, int>> &cmds, std::vector<std::string> &ops) {
    std::string cmd = "";
    int args = 0;
    for (int i = 0; i < input.size(); ++i) {
        if (input[i] == "<" || input[i] == "|" || input[i] == ">") {
            ops.push_back(input[i]);
            if (cmd != "") {
                cmds.push_back(std::pair<std::string, int>(cmd, args));
                cmd = "";
                args = 0;
            } 
        } else {
            cmd += input[i] + " ";
            args++;
            if (i == input.size()-1)
                cmds.push_back(std::pair<std::string, int>(cmd, args));
        }
    }
}

int tokenize(char *input, std::vector<std::string> &tokens) {
    int index = 0;
    int cmdc = 0;
    std::string delimeters = " \a\n\r\t";
    std::string meta_chars = "<|>&";
    std::string token_buffer = "";
    while (input[index] != '\0') {
        if (delimeters.find(input[index]) != std::string::npos) {
            if (token_buffer != "") {
                tokens.push_back(token_buffer);
                token_buffer = "";
            }
        } else if (meta_chars.find(input[index]) != std::string::npos) {
            if (token_buffer != "") {
                tokens.push_back(token_buffer);
                token_buffer = "";
            }
            tokens.push_back(std::string(1, input[index]));
            if (input[index] == '|')
                cmdc++;
        } else 
            token_buffer += input[index];
        index++;
    }
    if (tokens.size() != 0)
        cmdc++;
    return cmdc;
}

void cmd_convert(std::pair<std::string, int> &cmd_and_size, char **cmd) {
    int i = 0;
    std::string cmd_only = cmd_and_size.first;
    std::string cmd_buff;
    std::stringstream read_cmd(cmd_only);
    while(read_cmd >> cmd_buff) {
        cmd[i] = strdup(cmd_buff.c_str());
	    ++i;
    }
    cmd[i] = NULL;
}

void sig_handler(int signum) {
    while(waitpid(-1, 0, WNOHANG) > 0) {}
}

int changeDir(char **cmd){
	if (cmd[1] == NULL) {
		chdir(getenv("HOME")); 
		return EXIT_SUCCESS;
	} else { 
		if (chdir(cmd[1]) == -1) {
			printf(" %s: no such directory\n", cmd[1]);
            return EXIT_FAILURE;
		}
	}
	return 0;
}

void exec_cmds(std::vector<std::pair<std::string, int>> &cmds, std::string &ifile_str, std::string &ofile_str, bool bg) {
    int ifd, ofd, fd[2], next_pipe_in = 0, status;
    pid_t pid;
    char c;
    ssize_t bytes_read;

    bool err_read;
    int error[2];
    char err_c[1000];  
    err_c[0] = '\n';  

    for (int i = 0; i < cmds.size(); ++i) {
	    char *cmd[cmds[i].second + 1];
	    cmd_convert(cmds[i], cmd); 
        if (strcmp(cmd[0], "cd") == 0) {
                if(cmds.size() == 1) {
                    changeDir(cmd);
                    return;
                } else 
                    return;
        }
        
        pipe(error); // FOR ERROR
        
        if (pipe(fd) < 0) {
	        perror("ERROR: ");
	        exit(EXIT_FAILURE);
	    }
	    pid = fork();
	    if (pid < 0) {
	        perror("ERROR: ");
	        exit(EXIT_FAILURE);
	    } else if (pid == 0) {
	        // every pipe except first needs input from previous pipe's output
	        
            setpgid(0, 0); // FOR &


            close(error[0]); // FOR ERROR
            dup2(error[1], 2); // FOR ERROR
            close(error[1]); // FOR ERROR
            
            if (i == 0) {
	            if (close(fd[0]) < 0) {
	                perror("ERROR: ");
		            exit(EXIT_FAILURE);
	            }	
	        }
	        if (i == cmds.size()-1) {
	            if (close(fd[1]) < 0) {
		            perror("ERROR: ");
		            exit(EXIT_FAILURE);
		        }
	        }
	        if (i != 0) {
	            if (dup2(next_pipe_in, 0) < 0) {
		            perror("ERROR: ");
		            exit(EXIT_FAILURE);
		        }
		        if (close(next_pipe_in) < 0) {
		            perror("ERROR: ");
		            exit(EXIT_FAILURE);
		        }
	        }
	        if (i != cmds.size()-1) {
	            if (dup2(fd[1], 1) < 0) {
		            perror("ERROR: ");
		            exit(EXIT_FAILURE);
		        }
		        if (close(fd[1]) < 0) {
		            perror("ERROR: ");
		            exit(EXIT_FAILURE);
		        }
	        }
	        if (i == 0 && ifile_str != "") {
                ifd = open(ifile_str.c_str(), O_RDONLY, S_IRUSR);
                if (ifd < 0) {
                    perror("");
                    exit(EXIT_FAILURE);
                }
                if (dup2(ifd, 0) < 0) {
                    perror("ERROR: ");
                    exit(EXIT_FAILURE);
                }
                if (close(ifd) < 0) {
                    perror("ERROR: ");
                    exit(EXIT_FAILURE);
                }
            }
	        if (i == cmds.size()-1 && ofile_str != "") {
		        ofd = creat(ofile_str.c_str(), 0666);
		        if (ofd < 0) {
		            perror("ERROR: ");
		            exit(EXIT_FAILURE);
		        }
		        // while ((bytes_read = read(0, &c, sizeof(char))) > 0) {
		        //     if (write(ofd, &c, sizeof(char)) < 0) {
		        //         perror("ERROR: ");
			    //         exit(EXIT_FAILURE);
		        //     }
		        // }
		        // if (bytes_read < 0) {
		        //     perror("ERROR: ");
		        //     exit(EXIT_FAILURE);
		        // }	
                if (dup2(ofd, 1) < 0) {
                    perror("ERROR: ");
                    exit(EXIT_FAILURE);
                }
                if (close(ofd) < 0) {
                    perror("ERROR: ");
                    exit(EXIT_FAILURE);
                }
	        } 
	        // char *cmd[cmds[i].second + 1];
	        // cmd_convert(cmds[i], cmd);            
            if (execvp(cmd[0], cmd) < 0) {
		        perror("");
		        exit(EXIT_FAILURE);
	        }
        } else {
	        // if (bg) {
            //     signal(SIGCHLD, sig_handler);
            // } else {
            //     if (waitpid(pid, &status, 0) < 0) {
	        //         perror("ERROR: ");
		    //         exit(EXIT_FAILURE);
	        //     }
            // }
	        
            if (bg) {
                signal(SIGCHLD, sig_handler);
            } else {
            // PIPE STDERR
             close(error[1]);
             err_read = read(error[0], err_c, sizeof(err_c)-1);
             if (err_read > 0) {
                 int i = 0;
                 printf("ERROR: ");
                 while (err_c[i] != '\n') {
                    printf("%c", err_c[i]);
                    ++i;
                 }
                if (err_c[i] == '\n')
                    printf("%c", '\n');
             }
             close(error[0]);
            // PIPE STDERR
            
            next_pipe_in = fd[0];
	        /*if (close(fd[0]) < 0) {
	            perror("ERROR: ");
	            return;
	        }*/
	        if (close(fd[1]) < 0) {
	            perror("ERROR: ");
		        exit(EXIT_FAILURE);
	        }
	        /*if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
		        return;
	        }*/
            }
        }
    }
}

    
void prompt(bool print_prompt) {
    if (print_prompt)
        std::cout << "shell: ";
}

void handle_sigint(int sig) 
{ 
   return;
}

int main(int argc, char *argv[]) {	
    signal(SIGINT, handle_sigint);
    bool print_prompt = true;
    std::string n_str = "-n";
    const char *n = n_str.c_str(); 
    if (argc != 1 && strcmp(argv[1], n) == 0)
        print_prompt = false;
    do {
        char user_input[MAX_INPUT_SIZE];
	    std::vector<std::pair<std::string, int>> cmds;
        std::vector<std::string> ops;
        bool bg;
	    prompt(print_prompt);
	    if (fgets(user_input, MAX_INPUT_SIZE, stdin) == NULL)
	        return EXIT_FAILURE;
	    std::vector<std::string> input_tokens;    
        int cmdc = tokenize(user_input, input_tokens);        

	    // For testing tokenization before removing files
            /*std::cout << "Testing tokenization before removing files\n";
            for (int i = 0; i < input_tokens.size(); ++i) {
                std::cout << input_tokens[i] << "\n";
            }
            std::cout << "Num tokens: " << input_tokens.size() << '\n';
            std::cout << "Num commands: " << cmdc << '\n' << '\n';*/
	    // For testing tokenization before removing files
	
	    std::string ifile_str = find_ifile(input_tokens); // removes input files from user input if any
	    std::string ofile_str = find_ofile(input_tokens); // removes output files from user input if any

    	// For testing file recognition 
	    /*std::cout << "Testing file recognition after removing files\n";
        if (ifile_str != "")
	        std::cout << "Input file: " << ifile_str << '\n';
	    if (ofile_str != "")
	        std::cout << "Output file: " << ofile_str << '\n' << '\n'; 	*/
    	// For testing file recognition

        bg = run_in_bg(input_tokens);

        // For testing tokenization after removing files and & (if any)
        /*std::cout << "Testing tokenization after removing files and & (if any)\n";
        for (int i = 0; i < input_tokens.size(); ++i) {
            std::cout << input_tokens[i] << "\n";
        }
        std::cout << "Num tokens: " << input_tokens.size() << '\n';
        std::cout << "Num commands: " << cmdc << '\n';
        if (bg)
            std::cout << "Run in bg" << '\n' << '\n';
        else
            std::cout << "Run in fg" << '\n' << '\n';*/
	    // For testing tokenization after removing files and & (if any)

        parse_cmds(input_tokens, cmds, ops);

        // For testing parsed commands and operations
        /*std::cout << "Testing number of commands\n";
        std::cout << "cmdc = " << cmdc << '\n';
        std::cout << "cmds size = " << cmds.size() << '\n';

        std::cout << "Testing parsed commands\n";
        for (int i = 0; i < cmds.size(); ++i) {
            std::cout << "Command " << i+1 << " = " << cmds[i].first << "  Args = " << cmds[i].second << '\n';
        }
        std::cout << '\n' << "Testing operations\n";
        std::cout << "Ops =";
        for (int i = 0; i < ops.size(); ++i) {
            std::cout << " " << ops[i];
        }
        std::cout << '\n' << '\n';*/
        // For testing parsed commands and operations
    
	    // For testing command str -> char** conversions
	    /*std::cout << "Testing cmd str to char ** conversion\n";
	    bool loop;
	    for (int i = 0; i < cmds.size(); ++i) {
	        loop = true;
	        char *cmd[cmds[i].second + 1];
	        cmd_convert(cmds[i], cmd);
	        int j = 0;
	        std::cout << "Command String: " << cmds[i].first << '\n';
	        while(loop) {
	            if (cmd[j] == NULL) {
		            std::cout << "$" << '\n';
		            loop = false;
		        } else { 
		            std::cout << cmd[j] << '\n';
	    	        ++j;
		        }
	        }
	    }
	    std::cout << '\n';*/
	    // For testing command str -> char** conversions	
	
	    exec_cmds(cmds, ifile_str, ofile_str, bg);

    } while (!feof(stdin));
    
    return EXIT_SUCCESS;
}
