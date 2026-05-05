Part 1: Phase 1 (Binary Files & Permissions)

1. Prompts I Used
Prompt 1 (For parsing input): "Describe my record structure and generate a function int parse_condition(const char *input, char *field, char *op, char *value) which splits a field:operator:value string into three parts."
Prompt 2 (For matching records): "Describe the fields of my Report struct and generate a function int match_condition(Report *r, const char *field, const char *op, const char *value) which returns 1 if the record satisfies the condition and 0 otherwise."

2. What the AI Generated
The AI suggested using sscanf(input, "%[^:]:%[^:]:%s", ...) to parse the arguments using colons as delimiters. For matching, it gave me a bunch of basic if-else if blocks to compare the parsed command-line values against the actual fields inside my Report struct (like checking severity and category).

3. What I Changed and Why
The AI initially tried to compare everything—even numbers—using string comparisons. I rewrote this so that numeric fields (like severity) use atoi() and GPS coordinates use atof(). This was necessary so that math operators like >= or < actually work on the values.
 I noticed the AI's generated code didn't check string boundaries very well. I went back and added strncpy and manually checked strcmp boundaries to make sure the inspector names and categories wouldn't overflow our buffers.
Ensuring 208-Byte Struct Alignment: The AI didn't account for compiler padding on my 64-bit system, which originally bloated the struct to 216 bytes. I manually adjusted my char description[104] array size to offset the 8-byte alignment of the time_t timestamp. This kept the struct at exactly 208 bytes, which is vital for calculating correct record offsets.
I added a safety check to ensure sscanf actually returns 3. If it doesn't, the program warns the user about a malformed filter instead of silently failing or crashing.

4. What I Learned in Phase 1
I learned how to use sscanf with negated character sets to easily parse clean arguments. More importantly, I realized that writing to binary files requires a strict, predictable struct size. If your struct size fluctuates because of compiler padding, your lseek math and record indexing will completely break. I also got comfortable using stat() to verify UNIX file permission bits before letting any user action run.





Part 2: Phase 2 (Processes and Signals)

1. Prompts I Used
Prompt 3 (For Process Management): "How do I use fork() and execvp() to run an external command like rm -rf from within my C program, and why is wait() necessary?"
Prompt 4 (For Signals): "Show me how to implement sigaction to handle SIGUSR1 and SIGINT in a monitor program, ensuring the main process stays alive to receive multiple signals."

2. What the AI Generated
The AI gave me a basic template for a Fork-Exec pattern to call the system's rm binary. It also provided a template for the signal handling structure in the monitor, using struct sigaction and putting a pause() system call inside a while(1) loop to keep the process alive without maxing out CPU usage.

3. What I Changed and Why
The AI's suggested signal handlers had printf() inside them. I researched this and found out that printf isn't safe to use in handlers because it uses a global lock and buffers. If a signal hits while another printf is running, it can cause a deadlock. I rewrote the handlers to use the direct system call write(STDOUT_FILENO, ...) instead.The AI code assumed the monitor PID would never change. I made sure my city_manager reads the .monitor_pid file every single time a report is added. That way, if I close and restart the monitor in another terminal, the manager immediately grabs the new PID and sends the signal to the right place.
 I customized the signal response checks to match our project specification. If kill() fails or if .monitor_pid can't be opened, the program logs the specific failure (e.g., "Signal failed" or "PID file missing") directly to the district's text log file.

4. What I Learned in Phase 2
This phase taught me how Inter-Process Communication (IPC) actually works. Because signals are just basic interrupts that can't carry actual data, we had to use the file system (the .monitor_pid file) as a shared mailbox so the programs could find each other.
I also learned how the exec family of calls handles memory. Because execvp completely overwrites the calling process's memory space, I saw why we have to use fork() first. If we didn't, calling rm to delete a district would cause the city_manager itself to disappear and terminate. Finally, I learned how to use wait() in the parent process to keep child processes from lingering around as system "zombies," and how to initialize signal masks with sigemptyset to block unwanted interference.