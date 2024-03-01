#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unordered_map>

using namespace std;

// Operator structure to hold operation details
struct Operator {
    char type;
    string firstVar;
    string secondVar;
    string result;
};

// Pipe structure to facilitate main pipe implementation
struct Pipe {
    int readEnd;
    int writeEnd;

    Pipe() : readEnd(-1), writeEnd(-1) {}

    bool createPipe() {
        int fds[2];
        if (pipe(fds) != 0) {
            perror("pipe failed");
            return false;
        }
        readEnd = fds[0];
        writeEnd = fds[1];
        return true;
    }

    void closeReadEnd() {
        if (readEnd >= 0) {
            close(readEnd);
            readEnd = -1;
        }
    }

    void closeWriteEnd() {
        if (writeEnd >= 0) {
            close(writeEnd);
            writeEnd = -1;
        }
    }
};

// GLOBAL VARIABLES //
vector<string> inputVar;
vector<string> internalVar;
unordered_map<string, vector<Operator> > operationsMap;
unordered_map<string, int> variableValues;
vector<string> writeVariables;


/* 
    * The cleanParser() function removes leading and trailing 
    characters such as commas, semicolon, spaces

    * Variable string parser is the given string to clean

    * Return variable is the clean string

*/
string cleanParser(string parser) {

    while (!parser.empty() && (parser.front() == ',' || parser.front() == ';' || parser.front() == ' ')) {
        parser.erase(parser.begin());
    }
    while (!parser.empty() && (parser.back() == ',' || parser.back() == ';' || parser.back() == ' ')) {
        parser.pop_back();
    }

    return parser;
}


/* 
    * The parseInput() function parses the input file to extract variables and operations for graph-based computation

    * Populates global structures with input variables, internal variables, and operations

    * Variable string file_name contains the given dataflow graph

*/
void parseInput(string& file_name) {
    ifstream input(file_name);
    string line;    

    if (!input.is_open()) {
        throw runtime_error("Cannot open file: " + file_name);
    }

    while (getline(input, line, ';')) {
        istringstream iss(line);
        string parser;
        iss >> parser;

        if (parser == "input_var") {
            string varsLine;
            getline(iss, varsLine);
            istringstream varStream(varsLine);
            string var;
            while (getline(varStream, var, ',')) {
                var = cleanParser(var);
                if (!var.empty()) {
                    inputVar.push_back(var);
                }
            }
        }
        else if (parser == "internal_var") {
            string varsLine;
            getline(iss, varsLine);
            istringstream varStream(varsLine);
            string var;
            while (getline(varStream, var, ',')) {
                var = cleanParser(var);
                if (!var.empty()) {
                    internalVar.push_back(var);
                }
            }
        }

        else if (parser.find("write", 0) == 0) {
            // We use '(' bracket to know where to start
            int startPos = line.find('(');

            // We use ')' bracket to know where to end
            int endPos = line.find(')');

            if (startPos != string::npos && endPos != string::npos && endPos > startPos) {
                string varsStr = line.substr(startPos + 1, endPos - startPos - 1);

                istringstream varsStream(varsStr);
                string var;
                while (getline(varsStream, var, ',')) {
                    var = cleanParser(var);
                    if (!var.empty()) {
                        writeVariables.push_back(var);
                    }
                }
                
            } else {
                cerr << "Error: Parsing 'write' variables failed. Check syntax." << endl;
            }
            break;
        }

        else if (!parser.empty() && parser != "write") {
            Operator op;
            string arrow, targetVar;

            // We check if the line starts with an operation
            if (parser == "+" || parser == "-" || parser == "*" || parser == "/") {

                // Operation type
                op.type = parser[0]; 

                // First Variable
                iss >> targetVar;

                // Then, expect to find the "->" symbol
                // Followed by the destination variable
                iss >> arrow >> op.result;

                if (arrow != "->" || op.result.empty()) {
                    cerr << "Error: Parsing unary operation failed." << endl;
                    continue;
                }

                op.firstVar = cleanParser(targetVar);
            }else {
                
                // This section aims to remove outliers in the data
                string restOfLine;
                getline(iss, restOfLine);
                istringstream restStream(restOfLine);

                // Attempt to parse the arrow and result again using the new stream
                restStream >> arrow >> op.result;

                if (arrow != "->" || op.result.empty()) {
                    cerr << "Error: Expected '->' but found '" << arrow << "'" << endl;
                    continue;
                }

                // If we reach this section, parsing was successful
                op.firstVar = cleanParser(parser);
                op.type = '\0'; // detects null operation
                op.result = cleanParser(op.result);
            }

            op.result = cleanParser(op.result);

            // After parsing the operation line, add the operation to the operationsMap
            operationsMap[op.result].push_back(op);

            // Debugging print for the constructed operation
            cout << "Constructed operation: " << (op.type != '\0' ? string(1, op.type) + " " : "") << op.firstVar << " -> " << op.result << endl;
        }
    }

    input.close();
}


/* 
    * The initializeVars() function initializes given values needed to assign to the previous graph

    * Reads in values from a file using IO, and assigns them to corresponding variables

    * Variable string initialValue contains the given initialized values in a valid .txt file

*/
void initializeVars(const string& initialValue) {
    ifstream file(initialValue);
    if (!file.is_open()) {
        throw runtime_error("Cannot open file: " + initialValue);
    }

    string line;
    if (getline(file, line)) {
        istringstream iss(line);
        string value;
        int i = 0;

        while (getline(iss, value, ',')) {
            string cleanedValue = cleanParser(value);
            
            // Ensure we don't read more values than provided
            if (i >= inputVar.size()) {
                break;
            }

            string currentVarName = inputVar[i];
            variableValues[currentVarName] = stoi(cleanedValue);
            i++;
        }
    } else {
        cerr << "Error: Could not read the initial values line." << endl;
    }
    file.close();
}


/* 
     * Main function to orchestrate the execution of data flow operations.

    * Reads input specifications, initializes variables, creates pipes, and forks child processes to execute operations.

    * Results are read from pipes and written to an output file.

    * Values are given on the command line, values may contain important input files or output file name

*/
int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " [input-graph-file] [initial-values-file] [output-file-name]\n";
        return 1;
    }

    // Extract file paths from arguments.
    string dataFlow = argv[1], initialValues = argv[2], outputName = argv[3];

    // Sets up operations and dependencies
    parseInput(dataFlow);

    // Assigns initial values such as "x, y, z" with given inputs
    initializeVars(initialValues);

    // Pipe map to keep track of pipe information
    unordered_map<string, Pipe> operationPipes;
    vector<pid_t> childPids;

    // Create pipes for each internal variable
    for (const auto& var : internalVar) {
        Pipe pipe;
        if (pipe.createPipe()) {
            operationPipes[var] = move(pipe);
        } else {
            cerr << "Failed to create pipe for " << var << "\n";
            return EXIT_FAILURE;
        }
    }



    // The vector sorts the operations Map to be read starting from p0
    vector<string> sortedVars;
    for (const auto& pair : operationsMap) {
        sortedVars.push_back(pair.first);
    }
    sort(sortedVars.begin(), sortedVars.end());

    // Iterate over sorted variables to fork processes in the correct order
    for (const auto& var : sortedVars) {
        cout << "Forking for variable: " << var << "\n";
        pid_t pid = fork();

        if (pid == 0) { // Child process
            int result = 0;
            auto& ops = operationsMap[var];

            for (const auto& op : ops) {

                //Reads the first variable to be operated on
                int operandValue = variableValues[op.firstVar];

                // Compute result based on operation type
                switch (op.type) {
                    case '+':
                        result += operandValue;
                        break;
                    case '-':
                        result -= operandValue;
                        break;
                    case '*':
                        result *= operandValue;
                        break;
                    case '/':
                        if (operandValue == 0) {
                            cerr << "Error: Division by zero.\n";
                            exit(EXIT_FAILURE);
                        }
                        result /= operandValue;
                        break;
                    case '\0': // Direct assignment or no operation specified
                        result = operandValue;
                        break;
                    default:
                        cerr << "Unrecognized operation: " << op.type << "\n";
                        exit(EXIT_FAILURE);
                }
            }

            // Write the computed result to pipe
            if (operationPipes.find(var) != operationPipes.end()) {
                write(operationPipes[var].writeEnd, &result, sizeof(result));
                close(operationPipes[var].writeEnd);
            }

            exit(EXIT_SUCCESS);
        } else if (pid > 0) {
            // We check if the current execution context is within the parent process

            // We wait for child process to complete
            waitpid(pid, NULL, 0);

            // We then read the pipe result to calculate any variables that depend on a pipe answer
            if (operationPipes.find(var) != operationPipes.end()) {
                int result;
                int bytesRead = read(operationPipes[var].readEnd, &result, sizeof(result));
                if (bytesRead > 0) {

                    variableValues[var] = result;

                    cout << "Read result for " << var << ": " << result << endl;

                } else {
                    cerr << "Failed to read result for " << var << "\n";
                }
                // Close the read-end of the pipe.
                close(operationPipes[var].readEnd);
            }
        } else {
            cerr << "Failed to fork for " << var << "\n";
            return EXIT_FAILURE;
        }
    }


    // Wait for child processes to finish
    for (const auto& pid : childPids) {
        int status;
        waitpid(pid, &status, 0);
    }

    // Read results from pipes
    for (const auto& var : sortedVars) {
        if (operationPipes.find(var) != operationPipes.end()) {
            int result;
            read(operationPipes[var].readEnd, &result, sizeof(result));
        }
    }

    // Output results to the specified output file
    ofstream outFile(outputName);
    if (!outFile.is_open()) {
        cerr << "Failed to open output file.\n";
        return EXIT_FAILURE;
    }
    for (const auto& var : writeVariables) {

        if (find(inputVar.begin(), inputVar.end(), var) == inputVar.end()) {
            outFile << var << " = " << variableValues[var] << "\n";
        }

        // If you want to output both the Graph Input Variables and Initialized Variables use:
        /*
        outFile << var << " = " << variableValues[var] << "\n";
        */

    }
    outFile.close();

    cout << "Computation complete. Results written to " << outputName << ".\n";
    return 0;
}