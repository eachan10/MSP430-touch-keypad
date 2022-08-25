#include <windows.h>
#include <stdio.h>

#define Z_BIT 0x01
#define X_BIT 0x02

void PrintBits(char c);


int main() {
    // track keypresses
    char keypress = 0xFF;
    // setup for serial input
    DCB dcb;
    HANDLE com_handle;
    int f_success;
    const char* port = "COM4";
    DWORD dwEvtMask;
    char buffer[1];
    DWORD bytes_read;
    // for sending keystrokes
    INPUT inputs[2];

    com_handle = CreateFile(port,
                            GENERIC_READ | GENERIC_WRITE,   // read/write
                            0,                              // no sharing
                            NULL,                           // no security
                            OPEN_EXISTING,                  // existing port
                            0,                              // non-overlapped io
                            NULL                            // null for comm devices
    );

    if (com_handle == INVALID_HANDLE_VALUE){
        //  Handle the error.
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            printf("Device not connected");
            return 0;
        }
        printf ("CreateFile failed with error %d.\n", GetLastError());
        return (1);
    }

    // initialize dcb
    // SecureZeroMemory(&com_handle, sizeof(DCB));
    memset(&dcb, 0, sizeof(DCB));
    dcb.DCBlength = sizeof(DCB);

    f_success =  GetCommState(com_handle, &dcb);
    if (f_success == 0) {
        //  Handle the error.
        printf ("GetCommState failed with error %d.\n", GetLastError());
        return (2);
    }

    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    f_success = SetCommState(com_handle, &dcb);
    if (f_success == 0) {
        //  Handle the error.
        printf ("SetCommState failed with error %d.\n", GetLastError());
        return 1;
    }

    // set mask for receive
    f_success = SetCommMask(com_handle, EV_RXCHAR);
    if (f_success == 0) {
        // Handle the error.
        printf ("SetCommMask failed with the error %d.\n", GetLastError());
        return 1;
    }

    // wait for receive
    f_success = WaitCommEvent(com_handle, &dwEvtMask, NULL);
    if (f_success == 0) {
        // Handle the error.
        printf ("SetCommMask failed with the error %d.\n", GetLastError());
        return 1;
    }

    // setup for keyboard input simulation
    const KEYBDINPUT keydown_z = {
        .wVk = 0x5A,
    };
    const KEYBDINPUT keyup_z = {
        .wVk = 0x5A,
        .dwFlags = KEYEVENTF_KEYUP,
    };
    const KEYBDINPUT keydown_x = {
        .wVk = 0x58,
    };
    const KEYBDINPUT keyup_x = {
        .wVk = 0x58,
        .dwFlags = KEYEVENTF_KEYUP,
    };
    INPUT input_z_down, input_z_up, input_x_down, input_x_up;

    unsigned int sent;

    // finish setting up keyboard input
    input_z_down.type = INPUT_KEYBOARD; 
    input_z_up.type = INPUT_KEYBOARD;
    input_x_down.type = INPUT_KEYBOARD;
    input_x_up.type = INPUT_KEYBOARD;
    input_z_down.ki = keydown_z;
    input_z_up.ki = keyup_z;
    input_x_down.ki = keydown_x;
    input_x_up.ki = keyup_x;

    while (1) {
        // when the z or x bit is set it means not pressed

        // read 1 byte from the serial port
        f_success = ReadFile(com_handle, buffer, 1, &bytes_read, NULL);
        if (f_success == 0) {
            // Handle the error.
            if (GetLastError() == ERROR_OPERATION_ABORTED) {
                printf("Device disconnected");
                return 0;
            }
            printf ("ReadFile failed with the error %d.\n", GetLastError());
            return 1;
        }
        if ((buffer[0] & Z_BIT) == 0 && (keypress & Z_BIT)) {  // z sensor went from not pressed to pressed
            inputs[0] = input_z_down;
            keypress &= ~Z_BIT;
            sent = SendInput(1, inputs, sizeof(INPUT));
            if (sent != 1) {
                printf("SendInput failed");
                return 1;
            }
        } else if ((buffer[0] & Z_BIT) && (keypress & Z_BIT) == 0) {  // z sensor went from pressed to not pressed
            inputs[0] = input_z_up;
            keypress |= Z_BIT;
            sent = SendInput(1, inputs, sizeof(INPUT));
            if (sent != 1) {
                printf("SendInput failed");
                return 1;
            }
        }
        if ((buffer[0] & X_BIT) == 0 && (keypress & X_BIT)) {  // x sensor went from not pressed to pressed
            inputs[0] = input_x_down;
            keypress &= ~X_BIT;
            sent = SendInput(1, inputs, sizeof(INPUT));
            if (sent != 1) {
                printf("SendInput failed");
                return 1;
            }
        } else if ((buffer[0] & X_BIT) && (keypress & X_BIT) == 0) {  // x sensor went from pressed to not pressed
            inputs[0] = input_x_up;
            keypress |= X_BIT;
            sent = SendInput(1, inputs, sizeof(INPUT));
            if (sent != 1) {
                printf("SendInput failed");
                return 1;
            }
        }


    }


    CloseHandle(com_handle);  // cleanup
    return 0;
}


void PrintBits(char c) {
    int i;
    for (i = 0; i < 8; i++) {
        printf("%d", !!((c << i) & 0x80));
    }
}
