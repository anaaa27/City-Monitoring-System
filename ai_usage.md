 AI Usage Report - Phase 1

 1. Tool Used AI Model:
  Gemini 3 Flash.

  2. Prompts Provided
  Prompt 1 (Parsing): "Describe my record structure and generate a function int parse_condition(const char *input, char *field, char *op, char *value) which splits a field:operator:value string into three parts." .
  Prompt 2 (Matching): "Describe the fields of my Report struct and generate a function int match_condition(Report *r, const char *field, const char *op, const char *value) which returns 1 if the record satisfies the condition and 0 otherwise."

  3. What Was Generated
  The AI provided the logic for sscanf(input, "%[^:]:%[^:]:%s", ...) to parse the strings.It provided a series of if-else if statements to compare the Report struct fields (severity, category, etc.) against the user-provided values.
  
   4. What I Changed and Why 
    The AI initially compared all fields as strings. I changed the logic to use atoi(value) for the severity field and atof() for GPS coordinates to ensure mathematical operators (like >= or <) work correctly.String Safety: I added strncpy or verified that strcmp was used for the category and inspector name to prevent buffer overflows or incorrect memory comparisons.Error Handling: I added a check to ensure sscanf returns exactly 3, otherwise the program should warn the user about a malformed condition
   
   5. What I Learned
   I learned how to use formatted input scanning (sscanf) to break down complex command-line arguments.I learned that binary file records must have a fixed size (208 bytes in this case) so that the match_condition function can be applied consistently as we read the file.I learned the importance of programmatic permission management; even if the AI writes the logic, I must still use stat() to verify the file bits before calling the AI-generated matching functions


 Phase 2 - Processes and Signals

1. Prompts Provided
Prompt 3 (Process Management): "How do I use fork() and execvp() to run an external command like rm -rf from within my C program, and why is wait() necessary?"
Prompt 4 (Signals): "Show me how to implement sigaction to handle SIGUSR1 and SIGINT in a monitor program, ensuring the main process stays alive to receive multiple signals."

2. What Was Generated
The AI provided a template for the Fork-Exec pattern, showing how the child process's memory image is replaced by the rm command.
It provided a sigaction setup using struct sigaction and the pause() system call to wait for events without consuming CPU cycles.

3. What I Changed and Why
Async-Signal Safety: The AI initially suggested using printf() inside the signal handlers. Through further research, I learned that printf is not async-signal-safe (it uses a global lock and a buffer). I changed the Monitor's handler to use the write() system call directly to STDOUT_FILENO to prevent potential deadlocks.
Persistence: I modified the city_manager to read the .monitor_pid file every time a report is added. This ensures that if the monitor is restarted (and its PID changes), the manager always has the most current information.
Error Acknowledgment: Per the project requirements, I added specific log messages to the logged_district file to explicitly state if the monitor could not be informed (e.g., if kill() returns -1).

4. What I Learned
I learned that signals are a form of asynchronous notification. Since they don't carry data, I learned to use the file system (the .monitor_pid file) as a shared "mailbox" to facilitate the communication.
 learned that execvp does not return if it is successful, as it replaces the calling process. This is why the code for the rm command must be inside a fork() block; otherwise, the city_manager itself would be replaced by rm and exit immediately. I learned to use sigemptyset to initialize signal masks, ensuring that no stray signals interfere with the handling of SIGUSR1 or SIGINT.