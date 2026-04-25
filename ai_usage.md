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