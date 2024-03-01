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

// Pipe structure to facilitate main
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



string cleanParser(string parser) {

    while (!parser.empty() && (parser.front() == ',' || parser.front() == ';' || parser.front() == ' ')) {
        parser.erase(parser.begin());
    }
    while (!parser.empty() && (parser.back() == ',' || parser.back() == ';' || parser.back() == ' ')) {
        parser.pop_back();
    }

    return parser;
}


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
            /*
            cout << "Input Variables: ";
            for (const auto& var : inputVar) {
                cout << var << " ";
            }
            cout << endl;
            */
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
            /*
            cout << "Internal Variables: ";
            for (const auto& var : internalVar) {
                cout << var << " ";
            }
            cout << endl;
            */
        }

        else if (parser.find("write", 0) == 0) {
            size_t startPos = line.find('(');
            size_t endPos = line.find(')');
            if (startPos != string::npos && endPos != string::npos && endPos > startPos) {
                string varsStr = line.substr(startPos + 1, endPos - startPos - 1);
                cout << "Variables to write: " << varsStr << endl; // Debugging print

                istringstream varsStream(varsStr);
                string var;
                while (getline(varsStream, var, ',')) {
                    var = cleanParser(var); // Clean the variable name
                    cout << "Variable from write command: " << var << endl; // Debugging print
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

            // First, check if the line starts with an operation symbol for a unary operation
            if (parser == "+" || parser == "-" || parser == "*" || parser == "/") {
                op.type = parser[0]; // It's a unary operation

                // Read the next part, which should be the variable the operation applies to
                iss >> targetVar;

                // Then, expect to find the "->" symbol
                iss >> arrow >> op.result;

                if (arrow != "->" || op.result.empty()) {
                    cerr << "Error: Parsing unary operation failed." << endl;
                    continue;
                }

                // The operation applies directly to targetVar
                op.firstVar = cleanParser(targetVar);
            }else {
                // Assuming the format might have additional spaces, directly parsing the next segment might fail.
                // Consider reading the rest of the line and then parsing it with a new stringstream.
                string restOfLine;
                getline(iss, restOfLine);
                istringstream restStream(restOfLine);

                // Attempt to parse the arrow and result again using the new stream
                restStream >> arrow >> op.result;

                if (arrow != "->" || op.result.empty()) {
                    cerr << "Error: Expected '->' but found '" << arrow << "'" << endl;
                    continue; // Use continue to skip to the next line of input
                }

                // If reached here, parsing was successful
                op.firstVar = cleanParser(parser); // Clean and assign the first variable
                op.type = '\0'; // Indicating no explicit operation

                // Clean and assign the result variable name
                op.result = cleanParser(op.result);
            }

            // Clean the result name to ensure no spaces or invalid characters
            op.result = cleanParser(op.result);

            // After parsing the operation line, add the operation to the operationsMap
            operationsMap[op.result].push_back(op);

            // Debugging print for the constructed operation
            cout << "Constructed operation: " << (op.type != '\0' ? string(1, op.type) + " " : "") << op.firstVar << " -> " << op.result << endl;
        }
    }

    input.close();
}

void initializeVars(const string& initialValue) {
    ifstream file(initialValue);
    if (!file.is_open()) {
        throw runtime_error("Cannot open file: " + initialValue);
    }

    string line;
    if (getline(file, line)) {
        istringstream iss(line);
        string value;
        size_t i = 0;

        cout << "Reading initial values: '" << line << "'" << endl;
        while (getline(iss, value, ',')) {
            string cleanedValue = cleanParser(value);
            cout << "Raw value: '" << value << "'" << endl;
            cout << "Cleaned value: '" << cleanedValue << "'" << endl;
            
            // Ensure we don't read more values than provided or have variables to initialize
            if (i >= inputVar.size()) break;

            string currentVarName = inputVar[i];
            cout << "Current variable name: '" << currentVarName << "'" << endl;

            // Directly assign values to input variables; might need adjustment for internal variables
            cout << "Assigning " << currentVarName << " = '" << cleanedValue << "'" << endl;
            variableValues[currentVarName] = stoi(cleanedValue);

            i++;
        }
    } else {
        cerr << "Error: Could not read the initial values line." << endl;
    }

    file.close();

    // Optionally, initialize internal variables here if not done elsewhere
}


// Main logic to create processes based on the parsed graph
int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " [input specification] [initial values] [output file]\n";
        return 1;
    }

    string dataFlow = argv[1], initialValues = argv[2], outputName = argv[3];

    // Parse the input specification to set up operations and variables
    parseInput(dataFlow); // Sets up operations and dependencies
    initializeVars(initialValues); // Assigns initial values to x, y, z

    cout << "After Initialization:\n";
    for (const auto& var : variableValues) {
        cout << var.first << " = " << var.second << "\n";
    }

    cout << "Debugging prints for variable contents:\n";
    cout << "inputVar: ";
    for (const auto& var : inputVar) {
        cout << var << " ";
    }
    cout << "\n";

    cout << "internalVar: ";
    for (const auto& var : internalVar) {
        cout << var << " ";
    }
    cout << "\n";

    cout << "operationsMap:\n";
    for (const auto& [var, ops] : operationsMap) {
        cout << var << ": ";
        for (const auto& op : ops) {
            cout << "{" << op.type << ", " << op.firstVar << ", " << op.result << "} ";
        }
        cout << "\n";
    }

    cout << "variableValues:\n";
    for (const auto& [var, value] : variableValues) {
        cout << var << ": " << value << "\n";
    }



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



    // Fork child processes to perform operations
    vector<string> sortedVars;
    for (const auto& pair : operationsMap) {
        sortedVars.push_back(pair.first);
    }
    sort(sortedVars.begin(), sortedVars.end());

    cout << "sortedVars:\n";
    for (const auto& var : sortedVars) {
        cout << var << ": ";
        auto& ops = operationsMap[var];
        for (const auto& op : ops) {
            cout << "{" << op.type << ", " << op.firstVar << ", " << op.result << "} ";
        }
        cout << "\n";
    }

    cout << "Starting to fork child processes for operations in sorted order...\n";

    // Iterate over sorted variables to fork processes in the correct order
    for (const auto& var : sortedVars) {
        cout << "Forking for variable: " << var << "\n";
        pid_t pid = fork();

        if (pid == 0) { // Child process
            int result = 0; // Initialize result appropriately
            auto& ops = operationsMap[var]; // Get operations for this variable

            for (const auto& op : ops) {
                cout << "Processing operation: " << op.type << " " << op.firstVar << " -> " << var << "\n";
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
                cout << "Computed result for " << var << ": " << result << "\n";
            }

            // Write the computed result to pipe
            if (operationPipes.find(var) != operationPipes.end()) {
                write(operationPipes[var].writeEnd, &result, sizeof(result));
                close(operationPipes[var].writeEnd);
            }

            exit(EXIT_SUCCESS);
        } else if (pid > 0) {
            waitpid(pid, NULL, 0);

            if (operationPipes.find(var) != operationPipes.end()) {
                int result;
                ssize_t bytesRead = read(operationPipes[var].readEnd, &result, sizeof(result));
                if (bytesRead > 0) {

                    variableValues[var] = result;

                    cout << "Read result Before for " << var << ": " << result << endl;

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
        waitpid(pid, &status, 0); // Wait for each child process to finish
    }

    // Read results from pipes
    for (const auto& var : sortedVars) {
        if (operationPipes.find(var) != operationPipes.end()) {
            int result;
            read(operationPipes[var].readEnd, &result, sizeof(result));
        }
    }

    cout << "Final variable values:" << endl;
    for (const auto& pair : variableValues) {
        cout << pair.first << " = " << pair.second << endl;
    }

    // Output results to the specified file
    ofstream outFile(outputName);
    if (!outFile.is_open()) {
        cerr << "Failed to open output file.\n";
        return EXIT_FAILURE;
    }
    for (const auto& var : writeVariables) {

        if (find(inputVar.begin(), inputVar.end(), var) == inputVar.end()) {
            outFile << var << " = " << variableValues[var] << "\n";
        }

        // If you want to output the Input Variables and Answer use:
        /*
        outFile << var << " = " << variableValues[var] << "\n";
        */

    }
    outFile.close();

    cout << "Computation complete. Results written to " << outputName << ".\n";
    return 0;
}