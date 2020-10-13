#include <iostream>
#include <Windows.h>

class NumberReceiver {

    enum {
        NRS_BEFORE,
        NRS_DURING,
        NRS_AFTER,
        NRS_FAIL,
    };

public:

    int state = NRS_BEFORE;
    int value = 0;

    void reset(void) {

        value = 0;
        state = NRS_BEFORE;
    }

    BOOL ok() {

        return state == NRS_DURING || state == NRS_AFTER;
    }

    void receive(char c) {

        if (state == NRS_BEFORE) {

            if (c >= '0' && c <= '9') {

                state = NRS_DURING;
                value = c - '0';
                return;
            }

            if (c == ' ')
                return;
        }
        else
            if (state == NRS_DURING) {

                if (c >= '0' && c <= '9') {

                    value = 10 * value + c - '0';
                    return;
                }

                if (c == ' ') {

                    state = NRS_AFTER;
                    return;
                }
            }
            else
                if (state == NRS_AFTER) {

                    if (c == ' ')
                        return;
                }

        state = NRS_FAIL;
    }
};

#define dimof(arr) (sizeof(arr)/sizeof((arr)[0]))

HANDLE hCom;
DCB comParams;
char comReadBuffer[1024];
DWORD comReadLen;
COMMTIMEOUTS comTimeouts = { MAXDWORD, 0, 0, 0, 0 };
char keyPress;
FILE* fp_raw, *fp_dat;
NumberReceiver nr;

int dataState;
int dataIndex;

enum {
    DATA_STATE_FREE,
    DATA_STATE_INDEX,
    DATA_STATE_VALUE,
};

void LastErrorExit(const char *operation) {

    LPVOID lpMsgBuf;
    DWORD error = GetLastError();

    std::cout << operation << ": [error " << error << "] ";
    if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL)) {

        std::cout << "Error formatting error message." << std::endl;
    }
    else {

        std::cout << (TCHAR*)lpMsgBuf << std::endl;
        LocalFree(lpMsgBuf);
    }

    exit(error);
}

BOOL checkKeyPress(char* keyPress, int virtual_key) {

    if (GetKeyState(virtual_key) & 0x8000) {

        *keyPress = 1;
        return FALSE;
    }
    else 
    if (*keyPress == 0) {

        return FALSE;
    }
    else {

        *keyPress = 0;
        return (GetForegroundWindow() == GetConsoleWindow()) ? TRUE : FALSE;
    }
}

// 0x1b <index> 0x0f <value> ... <value> 0x0d/0x0a
void receiveChar(char c) {

    if (fp_raw)
        fwrite(&c, 1, 1, fp_raw);

    if (dataState == DATA_STATE_FREE) {

        if (c == 0x1b) {

            dataState = DATA_STATE_INDEX;
            nr.reset();
        }
    }
    else
    if (dataState == DATA_STATE_INDEX) {

        if (c == 0x0f) {

            if (nr.ok()) {

                if (dataIndex != nr.value)
                    std::cout << "index out of order, expected " << dataIndex << ", received " << nr.value << std::endl;

                dataIndex = nr.value;
            }
            else {

                std::cout << "index error" << std::endl;
            }

            nr.reset();
            dataState = DATA_STATE_VALUE;
        }
        else
            nr.receive(c);
    }
    else
    if (dataState == DATA_STATE_VALUE) {

        if (c == 0x0f || c == 0x0d || c == 0x0a) {

            if (!nr.ok()) {

                std::cout << "value error" << std::endl;
            }
            else
            if (fp_dat) {

                fprintf(fp_dat, "%d, %d\n", dataIndex++, nr.value);
            }

            nr.reset();
            dataState = (c == 0x0f) ? DATA_STATE_VALUE : DATA_STATE_FREE;
        }
        else
            nr.receive(c);
    }

    std::cout << "channel = " << dataIndex << '\r';
}

int main(int argc, char* argv[]) {

    if (argc < 3) {

        std::cout << "Usage: " << argv[0] << " COM<x> <mode>" << std::endl;
        exit(-1);
    }

    if (BuildCommDCB(argv[2], &comParams) == FALSE) {

        LastErrorExit("Building DCB");
    }

    if ((hCom = CreateFile(argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE) {

        LastErrorExit("Creating port file");
    }

    if (SetCommState(hCom, &comParams) == FALSE) {

        CloseHandle(hCom);
        LastErrorExit("Setting port mode");
    }

    if (SetCommTimeouts(hCom, &comTimeouts) == FALSE) {

        CloseHandle(hCom);
        LastErrorExit("Setting port timeouts");
    }

    // Wait until the initial "enter" pressed to start the program, is released.
    while ((GetKeyState(VK_RETURN) & 0x8000) && GetForegroundWindow() == GetConsoleWindow())
        ;

    // If output to file is desired, open the file.
    if (argc == 4) {

        if (fopen_s(&fp_raw, std::string(argv[3]).append(".raw").c_str(), "wb")) {

            CloseHandle(hCom);
            LastErrorExit("Opening .raw output file");
        }

        if (fopen_s(&fp_dat, std::string(argv[3]).append(".csv").c_str(), "w")) {

            CloseHandle(hCom);
            fclose(fp_raw);
            LastErrorExit("Opening .dat output file");
        }
    }

    // Read the data until <enter> key is pressed.
    dataState = DATA_STATE_FREE;
    std::cout << "Reading from " << argv[1] << " port. Press <ENTER> to stop." << std::endl;
    while (!checkKeyPress(&keyPress, VK_RETURN) && dataIndex < 1024) {

        (void)ReadFile(hCom, comReadBuffer, sizeof(comReadBuffer), &comReadLen, NULL);
        for (decltype(comReadLen) i = 0; i < comReadLen; i++)
            receiveChar(comReadBuffer[i]);

        Sleep(10);
    }

    // Close file that was written into.
    if (fp_raw)
        fclose(fp_raw);
    if (fp_dat)
        fclose(fp_dat);

    // Close COM handle.
    CloseHandle(hCom);
    std::cout << std::endl << "Finished reading." << std::endl;
    return 0;
}
/*
"2400 even 8 2" -> "baud=2400 parity=e data=8 stop=2"
"9600 even 7 2" -> "baud=9600 parity=e data=7 stop=2"
*/
